#include "UsbDeviceManager.h"
#include "MainWindow.h"
#include "Safe.h"
#include <QJsonDocument>
#include <magic_enum/magic_enum.hpp>
#include <QEventLoop>
#include <QStringList>

UsbDeviceManager::UsbDeviceManager(QObject* parent) : QObject(parent)
{
    usbThreadPool = new QThreadPool(this);
    usbThreadPool->setMaxThreadCount(MAX_CONCURRENT_CONNECTIONS * 2);

    watcher = new QFutureWatcher<QSet<QString>>(this);
    connect(watcher, &QFutureWatcher<QSet<QString>>::finished, this, &UsbDeviceManager::handlePollFinished);

    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &UsbDeviceManager::pollDevices);

    connectTimer = new QTimer(this);
    connect(connectTimer, &QTimer::timeout, this, &UsbDeviceManager::connectPendingDevices);
}

void UsbDeviceManager::start() {
    qInfoEx() << "🚀 启动设备管理器...";

    pollDevices();

    pollTimer->start(2000);
    connectTimer->start(2000);
}

void UsbDeviceManager::stop() {
    qInfoEx() << "🛑 停止设备管理器...";

    pollTimer->stop();
    connectTimer->stop();
    watcher->disconnect();
    
    connectionQueue.clear(); // 清空排队中的连接任务

    // 如果后台线程正在运行，必须等待其结束，否则程序退出时可能会崩溃
    if (watcher->isRunning())
        watcher->waitForFinished();

    const auto connections = connToContext.keys();
    for (auto conn : connections) {
        disconnectDevice(conn);
    }

    devices.clear();
    previousDevices.clear();
    missingPollCounts.clear();
    deviceBuffers.clear();
}

void UsbDeviceManager::connectPendingDevices() {
    bool isUsbSetting = MainWindow::getInstance()->getTab().getConnectionMethod() == 0;

    QStringList pendingUdids;
    for (auto it = devices.constBegin(); it != devices.constEnd(); ++it) {
        if (!it.value()) {
            pendingUdids.append(it.key());
        }
    }

    for (const QString& udid : pendingUdids) {
        // 检查是否已经有对应的 Context 正在连接中或已连接
        // 避免定时器和热插拔事件同时触发导致重复连接
        bool contextExists = false;
        for (auto ctx : connToContext) {
            if (ctx->udid == udid && ctx->port == 32839) {
                contextExists = true;
                break;
            }
        }
        if (contextExists) continue;

        if (DeviceInfo::isLockByOther(udid))
            continue;

        auto deviceInfo = DeviceInfo::getDevice(udid);

        if (!deviceInfo || (deviceInfo->connection->type != DeviceConnection::Usb && isUsbSetting)) {
            devices[udid] = true; // 标记处理中，防止重复执行
            connectDevice(udid, 32839, false); // 这会将任务推入排队系统
        }
    }
}

DeviceConnection* UsbDeviceManager::connectDevice(const QString& udid, uint16_t port, bool rawMode) {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    UsbDeviceContext* ctx = new UsbDeviceContext();
    ctx->udid = udid;
    ctx->port = port;

    ctx->handler = new DeviceConnection((idevice_connection_t)nullptr);
    DeviceConnection* conn = ctx->handler;
    
    connToContext.insert(conn, ctx);
    if (port == 32839) {
        devices[udid] = true; // 标记正在处理
    }

    // 将连接请求推入队列进行延时调度（初始重试次数为0）
    connectionQueue.enqueue({udid, port, rawMode, QPointer<DeviceConnection>(conn), ctx, 0});
    processConnectionQueue();

    return conn; 
#else
    return nullptr;
#endif
}

void UsbDeviceManager::processConnectionQueue() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // 只要没有达到并发限制且队列中有任务，就继续派发
    while (pendingConnections < MAX_CONCURRENT_CONNECTIONS && !connectionQueue.isEmpty()) {
        ConnectionTask task = connectionQueue.dequeue();

        // 如果排队期间设备被拔出，QPointer 会变为空，直接跳过
        if (!task.conn) {
            continue;
        }

        DeviceConnection* rawConn = task.conn.data();
        UsbDeviceContext* actualCtx = connToContext.value(rawConn, nullptr);
        
        // 校验上下文一致性，防止地址复用
        if (!actualCtx || actualCtx != task.ctx) {
            continue;
        }

        pendingConnections++; // 增加并发记录

        QString udid = task.udid;
        uint16_t port = task.port;
        bool rawMode = task.rawMode;
        int retries = task.retries;
        QPointer<DeviceConnection> safeConn = task.conn;

        using ConnResult = std::tuple<bool, idevice_t, idevice_connection_t>;
        QFutureWatcher<ConnResult>* watcher = new QFutureWatcher<ConnResult>(this);
        
        // 使用独立的 usbThreadPool 执行阻塞的连接任务
        auto future = QtConcurrent::run(usbThreadPool, [udid, port]() -> ConnResult {
            idevice_t device = nullptr;
            idevice_connection_t connection = nullptr;
            
            QByteArray udidBytes = udid.toUtf8(); 
            if (IDEVICE_E_SUCCESS != idevice_new_with_options(&device, udidBytes.constData(), IDEVICE_LOOKUP_USBMUX)) {
                return {false, nullptr, nullptr};
            }
            if (idevice_connect(device, port, &connection) != IDEVICE_E_SUCCESS) {
                idevice_free(device);
                return {false, nullptr, nullptr};
            }
            return {true, device, connection};
        });

        // 监听线程结束信号，回到主线程执行真正的事件绑定
        connect(watcher, &QFutureWatcher<ConnResult>::finished, this, [=]() {
            pendingConnections--; // 当前线程结束，释放一个并发名额
            processConnectionQueue(); // 触发下一个排队任务

            ConnResult res = watcher->result();
            watcher->deleteLater();

            bool success = std::get<0>(res);
            idevice_t device = std::get<1>(res);
            idevice_connection_t connection = std::get<2>(res);

            // 检查连接对象是否在排队或连接期间被拔出销毁
            if (!safeConn) {
                if (success) { // 如果确实连上了，由于上层已丢弃，必须立刻断开防止泄露
                    idevice_disconnect(connection);
                    idevice_free(device);
                }
                return;
            }

            DeviceConnection* currentConn = safeConn.data();
            UsbDeviceContext* ctx = connToContext.value(currentConn, nullptr);

            if (!ctx || ctx->udid != udid) {
                if (success) {
                    idevice_disconnect(connection);
                    idevice_free(device);
                }
                return;
            }

            // 【关键修复】应对 100+ 设备并发时，苹果 AMDS 服务随机丢弃子端口握手的问题
            if (!success) {
                if (retries < 5) {
                    qDebugEx() << QString("⚠️ 端口 %1 被系统拒载，准备第 %2 次重试: %3").arg(port).arg(retries + 1).arg(udid);
                    
                    // 延迟 1 秒后重新塞入队列重试，不报错给前端，完全静默拦截掉线
                    QTimer::singleShot(1000, this, [=]() {
                        // 确保这 1 秒期间设备没有被拔掉
                        if (safeConn && connToContext.value(safeConn.data(), nullptr) == ctx) {
                            connectionQueue.enqueue({udid, port, rawMode, safeConn, ctx, retries + 1});
                            processConnectionQueue();
                        }
                    });
                    return; // 暂不清理对象，等待重试
                }

                // 超过最大重试次数，才彻底向上层抛出错误并清理
                emit errorOccurred(currentConn, QString("连接端口 %1 失败超过最大重试次数: %2").arg(port).arg(udid));
                if (port == 32839 && devices.contains(udid)) devices[udid] = false; 
                
                connToContext.remove(currentConn);
                currentConn->deleteLater();
                delete ctx;
                return;
            }

            ctx->device = device;
            ctx->connection = connection;

            currentConn->setConnection(ctx->connection);
            
            if (!rawMode) {
                currentConn->send("deviceInfo", QJsonObject{
                    {"remoteDeviceName", QHostInfo::localHostName()}
                });
            }

            int fd = -1;
            if (idevice_connection_get_fd(ctx->connection, &fd) == IDEVICE_E_SUCCESS && fd >= 0) {
                ctx->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, currentConn);
                
                connect(ctx->notifier, &QSocketNotifier::activated, currentConn, [=](int) {
                    if (!safeConn || connToContext.value(currentConn, nullptr) != ctx) return; 
                    
                    quint32 bytes = 0;
                    idevice_error_t err = idevice_connection_receive(ctx->connection, ctx->readBuffer.data(), ctx->readBuffer.size(), &bytes);
                    
                    if (err == IDEVICE_E_SUCCESS && bytes > 0) {
                        if (rawMode) {
                            const QByteArray rawData(ctx->readBuffer.constData(), bytes);
                            currentConn->dispatchUsbRawData(rawData);
                            return;
                        }

                        deviceBuffers[ctx].append(ctx->readBuffer.constData(), bytes);
                        processBufferedData(ctx);
                    } else if (err != IDEVICE_E_SUCCESS) {
                        emit errorOccurred(currentConn, QString("%1端口通信错误: %2").arg(port).arg(magic_enum::enum_name(err)));
                        disconnectDevice(currentConn);
                    }
                });
            }

            if (ctx->port == 32839) {
                emit deviceConnected(currentConn);
            }

            qDebugEx() << "✅连接设备:" << ctx->udid + ":" + QString::number(ctx->port);
        });

        watcher->setFuture(future);
    }
#endif
}

void UsbDeviceManager::disconnectDevice(DeviceConnection* conn) {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (!conn) return;

    UsbDeviceContext* ctx = connToContext.value(conn, nullptr);
    if (!ctx) return;

    if (ctx->handler != conn) {
        qCriticalEx() << "disconnectDevice" << conn;
        return;
    }

    connToContext.remove(conn);

    if (ctx->port == 32839) {
        if (devices.contains(ctx->udid))
            devices[ctx->udid] = false;

        emit deviceDisconnected(conn);
    }

    qDebugEx() << "❌断开设备:" << ctx->udid + ":" + QString::number(ctx->port);

    if (ctx->notifier) {
        delete ctx->notifier;
        ctx->notifier = nullptr;
    }

    // 必须在后台异步释放之前，清空当前前台连接句柄。防止并发时还在 write 发送数据导致底层直接闪退！
    ctx->handler->setConnection(nullptr); 
    ctx->handler->deleteLater();

    auto connection = ctx->connection;
    auto device = ctx->device;
    
    // 异步释放底层资源
    QtConcurrent::run(usbThreadPool, [connection, device]() {
        if (connection) idevice_disconnect(connection);
        if (device) idevice_free(device);
    });

    deviceBuffers.remove(ctx);
    delete ctx;
#endif
}

void UsbDeviceManager::pollDevices() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (MainWindow::getInstance()->getTab().getAutoConnectUSBDevices() == 0)
        return;

    if (watcher->isRunning())
        return; // 避免多次并发轮询

    auto future = QtConcurrent::run(usbThreadPool, []() -> QSet<QString> {
        idevice_info_t* deviceList = nullptr;
        int count = 0;

        if (idevice_get_device_list_extended(&deviceList, &count) != IDEVICE_E_SUCCESS) {
            qCriticalEx() << "⚠️ 获取设备列表失败";
            return {};
        }

        QSet<QString> currentDevices;
        for (int i = 0; i < count; i++) {
            const auto* info = deviceList[i];
            if (!info || info->conn_type != CONNECTION_USBMUXD || !info->udid) {
                continue;
            }

            QString udid = QString::fromUtf8(info->udid);
            currentDevices.insert(udid);
        }

        idevice_device_list_extended_free(deviceList);
        return currentDevices;
    });

    watcher->setFuture(future);
#endif
}

void UsbDeviceManager::handlePollFinished() {
    QSet<QString> currentDevices = watcher->result();
    bool needConnect = false;

    for (const QString& udid : currentDevices) {
        missingPollCounts.remove(udid);

        if (!previousDevices.contains(udid)) {
            qInfoEx() << "📱检测到新设备:" << udid;
            devices[udid] = false;
            needConnect = true;
        }
    }

    if (needConnect)
        connectPendingDevices();

    QSet<QString> trackedUdids;
    for (auto it = devices.cbegin(); it != devices.cend(); ++it)
        trackedUdids.insert(it.key());
    for (const auto& ctx : connToContext)
        trackedUdids.insert(ctx->udid);

    QSet<QString> confirmedRemovedUdids;
    for (const QString& udid : trackedUdids) {
        if (currentDevices.contains(udid))
            continue;

        int missingCount = missingPollCounts.value(udid, 0) + 1;
        missingPollCounts[udid] = missingCount;

        if (missingCount < MISSING_POLLS_BEFORE_DISCONNECT) {
            qDebugEx() << "USB轮询暂时缺席，等待下次确认:" << udid << missingCount;
            continue;
        }

        confirmedRemovedUdids.insert(udid);
        missingPollCounts.remove(udid);
    }

    QList<UsbDeviceContext*> list;
    for (const auto& ctx : connToContext) {
        if (confirmedRemovedUdids.contains(ctx->udid))
            list.append(ctx);
    }

    for (auto ctx : list) {
        qInfoEx() << "❌检测到设备拔出:" << ctx->udid;
        devices.remove(ctx->udid);
        disconnectDevice(ctx->handler);
    }

    for (const QString& udid : confirmedRemovedUdids)
        devices.remove(udid);

    previousDevices = currentDevices;
}

void UsbDeviceManager::processBufferedData(UsbDeviceContext* ctx) {
    DeviceConnection* conn = ctx->handler;

    while (connToContext.contains(conn) && deviceBuffers.contains(ctx)) {
        auto &buffer = deviceBuffers[ctx];

        if (buffer.size() < sizeof(quint64) + sizeof(quint32))
            return;

        auto identifier = *reinterpret_cast<const quint64*>(buffer.constData());
        if (identifier != 0xb7c2e0f542a39a3e) {
            qCriticalEx() << HIDE_STR("识别码不匹配，清空缓冲区");
            buffer.clear();
            return;
        }

        auto size = *reinterpret_cast<const quint32*>(buffer.constData() + sizeof(quint64));
        
        if (buffer.size() < static_cast<int>(sizeof(quint64) + sizeof(quint32) + size)) {
            // qDebugEx() << "数据不完整，等待更多数据";
            return;
        }

        const auto data = buffer.mid(sizeof(quint64) + sizeof(quint32), size);
        buffer.remove(0, sizeof(quint64) + sizeof(quint32) + size);

        const auto& jsonData = AesCrypto::decrypt(data);
        if (jsonData.size() == 0) {
            qDebugEx() << HIDE_STR("解密失败");
            return;
        }

        const auto& doc = QJsonDocument::fromJson(jsonData);

        if (!doc.isNull())
            emit dataReceived(conn, doc.object());
        else
            qCriticalEx() << HIDE_STR("JSON 解析失败，丢弃数据");
    }
}
