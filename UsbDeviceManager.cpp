#include "UsbDeviceManager.h"
#include "MainWindow.h"
#include "Safe.h"
#include <QJsonDocument>
#include <magic_enum/magic_enum.hpp>
#include <QEventLoop>

UsbDeviceManager::UsbDeviceManager(QObject* parent) : QObject(parent)
{
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
    
    // 如果后台线程正在运行，必须等待其结束，否则程序退出时可能会崩溃
    if (watcher->isRunning())
        watcher->waitForFinished();

    const auto connections = connToContext.keys();
    for (auto conn : connections) {
        disconnectDevice(conn);
    }

    devices.clear();
    previousDevices.clear();
    deviceBuffers.clear();
}

void UsbDeviceManager::connectPendingDevices() {
    bool isUsbSetting = MainWindow::getInstance()->getTab().getConnectionMethod() == 0;

    for (auto it = devices.keyValueBegin(); it != devices.keyValueEnd(); ++it) {
        if (it->second)
            continue;

        QString udid = it->first;

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

        if (!deviceInfo || deviceInfo->connection->type != DeviceConnection::Usb && isUsbSetting) {
            devices[udid] = true; // 标记处理中，防止重复执行
            connectDevice(udid, 32839, false);
            QTimer::singleShot(0, this, &UsbDeviceManager::connectPendingDevices);
            break;
        }
    }
}

DeviceConnection* UsbDeviceManager::connectDevice(const QString& udid, uint16_t port, bool rawMode) {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // 耗时握手交由后台执行，主线程用 QEventLoop 挂起等待
    auto future = QtConcurrent::run([=]() -> UsbDeviceContext* {
        UsbDeviceContext* ctx = new UsbDeviceContext();
        ctx->udid = udid;
        ctx->port = port;

        if (IDEVICE_E_SUCCESS != idevice_new(&ctx->device, udid.toUtf8().constData())) {
            delete ctx;
            return nullptr;
        }

        if (idevice_connect(ctx->device, port, &ctx->connection) != IDEVICE_E_SUCCESS) {
            idevice_free(ctx->device);
            delete ctx;
            return nullptr;
        }
        return ctx;
    });

    // 挂起主线程但保持 UI 刷新与响应
    QFutureWatcher<UsbDeviceContext*> futureWatcher;
    QEventLoop loop;
    connect(&futureWatcher, &QFutureWatcher<UsbDeviceContext*>::finished, &loop, &QEventLoop::quit);
    futureWatcher.setFuture(future);
    loop.exec(); // 停在这里等待，直到后台连上

    UsbDeviceContext* ctx = futureWatcher.result();
    
    if (!ctx) {
        emit errorOccurred(nullptr, QString("无法连接端口 %1 的设备: %2").arg(port).arg(udid));
        if (port == 32839 && devices.contains(udid)) devices[udid] = false; // 允许后续重试
        return nullptr;
    }

    // 后台连上后，回到主线程安全地绑定所有 Qt UI 对象
    ctx->handler = new DeviceConnection(ctx->connection);
    
    if (!rawMode) {
        ctx->handler->send("deviceInfo", QJsonObject{
            {"remoteDeviceName", QHostInfo::localHostName()}
        });
    }

    int fd = -1;
    if (idevice_connection_get_fd(ctx->connection, &fd) == IDEVICE_E_SUCCESS && fd >= 0) {
        ctx->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, ctx->handler);
        connect(ctx->notifier, &QSocketNotifier::activated, [=](int) {
            QByteArray buffer(1024 * 1024, Qt::Uninitialized);
            quint32 bytes = 0;
            idevice_error_t err = idevice_connection_receive(ctx->connection, buffer.data(), buffer.size(), &bytes);
            if (err == IDEVICE_E_SUCCESS && bytes > 0) {
                QByteArray data = buffer.left(bytes);

                if (rawMode) {
                    emit rawDataReceived(ctx->handler, data);
                    return;
                }

                qDebugEx() << "接收到字节数据" << data.size();
                deviceBuffers[ctx].append(data);
                processBufferedData(ctx);
            } else if (err != IDEVICE_E_SUCCESS) {
                emit errorOccurred(ctx->handler, QString("%1端口通信错误: %2").arg(port).arg(magic_enum::enum_name(err)));
                disconnectDevice(ctx->handler);
            }
        });
    }

    connToContext.insert(ctx->handler, ctx);

    if (ctx->port == 32839) {
        devices[udid] = true;
        emit deviceConnected(ctx->handler);
    }

    qDebugEx() << "✅连接设备:" << ctx->udid + ":" + QString::number(ctx->port);
    
    return ctx->handler;
#else
    return nullptr;
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

    ctx->handler->deleteLater();

    auto connection = ctx->connection;
    auto device = ctx->device;
    QtConcurrent::run([connection, device]() {
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

    auto future = QtConcurrent::run([=]() -> QSet<QString> {
        char** deviceList = nullptr;
        int count = 0;

        if (idevice_get_device_list(&deviceList, &count) != IDEVICE_E_SUCCESS) {
            qCriticalEx() << "⚠️ 获取设备列表失败";
            return {};
        }

        QSet<QString> currentDevices;
        for (int i = 0; i < count; i++) {
            QString udid = QString::fromUtf8(deviceList[i]);
            currentDevices.insert(udid);
        }

        idevice_device_list_free(deviceList);
        return currentDevices;
    });

    watcher->setFuture(future);
#endif
}

void UsbDeviceManager::handlePollFinished() {
    QSet<QString> currentDevices = watcher->result();
    bool needConnect = false;

    for (const QString& udid : currentDevices) {
        if (!previousDevices.contains(udid)) {
            qInfoEx() << "📱检测到新设备:" << udid;
            devices[udid] = false;
            needConnect = true;
        }
    }

    if (needConnect)
        connectPendingDevices();

    QList<UsbDeviceContext*> list;
    
    for (const auto& ctx : connToContext) {
        if (!currentDevices.contains(ctx->udid)) {
            list.append(ctx);
        }
    }

    for (auto ctx : list) {
        qInfoEx() << "❌检测到设备拔出:" << ctx->udid;
        devices.remove(ctx->udid);
        disconnectDevice(ctx->handler);
    }

    previousDevices = currentDevices;
}

void UsbDeviceManager::processBufferedData(UsbDeviceContext* ctx) {
    while (deviceBuffers.contains(ctx)) {
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

        const auto& data = buffer.mid(sizeof(quint64) + sizeof(quint32), size);
        buffer.remove(0, sizeof(quint64) + sizeof(quint32) + size);

        const auto& jsonData = AesCrypto::decrypt(data);
        if (jsonData.size() == 0) {
            qDebugEx() << HIDE_STR("解密失败");
            return;
        }

        const auto& doc = QJsonDocument::fromJson(jsonData);

        if (!doc.isNull())
            emit dataReceived(ctx->handler, doc.object());
        else
            qCriticalEx() << HIDE_STR("JSON 解析失败，丢弃数据");
    }
}
