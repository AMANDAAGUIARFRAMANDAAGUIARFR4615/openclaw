#pragma once

#include "AesCrypto.h"
#include "UsbDeviceManager.h"
#include <magic_enum/magic_enum.hpp>
#include <QByteArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <QHash>
#include <QList>

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#include <libimobiledevice/libimobiledevice.h>
#endif

class DeviceInfo;

class DeviceConnection : public QObject
{
    Q_OBJECT

public:
    enum Type {
        Usb,
        Tcp
    };

    explicit DeviceConnection(QTcpSocket *socket) : type(Tcp), tcpSocket(socket), QObject(socket) {}
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    explicit DeviceConnection(idevice_connection_t connection) : type(Usb), usbConnection(connection), QObject(nullptr) {}

    void setConnection(idevice_connection_t connection) { 
        usbConnection = connection; 
        if (usbConnection) {
            flushQueuedUsbWrites();
            flushQueuedUsbRawInputs();
            emit connected();
        }
    }

    void dispatchUsbRawData(const QByteArray &data) {
        if (!usbConnection) {
            queuedUsbRawInputs.append(data);
            return;
        }
        emit usbRawDataReceived(data);
    }
#endif

    void send(const DataGuard::StrObfuscator<>& event, const QJsonValue &jsonValue = QJsonValue()) {
        QJsonObject jsonObject;
        jsonObject["event"] = event.decrypt();

        if (!jsonValue.isUndefined() && !jsonValue.isNull())
            jsonObject["data"] = jsonValue;

        qDebugEx() << this << jsonObject;

        if (type == Tcp && tcpSocket->state() != QAbstractSocket::ConnectedState) {
            qCriticalEx() << "不是连接状态，无法发送数据";
            return;
        }

        quint64 identifier = 0xc6e8f3de9a654d6b;

        QByteArray jsonData = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
        const auto& data = AesCrypto::encrypt(jsonData);

        quint32 size = data.size();

        QByteArray dataToSend;
        dataToSend.append(reinterpret_cast<const char*>(&identifier), sizeof(identifier));
        dataToSend.append(reinterpret_cast<const char*>(&size), sizeof(size));
        dataToSend.append(data);

        write(dataToSend);
    }

    void write(const QByteArray &byteArray) {
        if (type == Tcp)
        {
            tcpSocket->write(byteArray);
            tcpSocket->flush();
        }
        else
        {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
            if (!usbConnection) {
                queuedUsbWrites.append(byteArray);
                return;
            }

            sendUsbBytes(byteArray);
#endif
        }
    }

    void close() {
        qDebugEx() << "close" << type;

        if (type == Tcp)
            tcpSocket->disconnectFromHost();
        else
            UsbDeviceManager::getInstance()->disconnectDevice(this);
    }

    const Type type;

    DeviceInfo* deviceInfo = nullptr;

signals:
    void connected();
    /** USB raw 模式（如投屏、文件隧道）收到的字节流；按连接分发，避免全局 rawData 信号导致 O(N²) 槽调用。 */
    void usbRawDataReceived(const QByteArray &data);

protected:
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    void flushQueuedUsbWrites() {
        while (!queuedUsbWrites.isEmpty() && usbConnection) {
            if (!sendUsbBytes(queuedUsbWrites.takeFirst()))
                break;
        }
    }

    void flushQueuedUsbRawInputs() {
        while (!queuedUsbRawInputs.isEmpty() && usbConnection)
            emit usbRawDataReceived(queuedUsbRawInputs.takeFirst());
    }

    bool sendUsbBytes(const QByteArray &byteArray) {
        int totalSent = 0;
        const int totalSize = byteArray.size();

        while (totalSent < totalSize) {
            quint32 sent = 0;
            idevice_error_t error = idevice_connection_send(usbConnection, byteArray.constData() + totalSent, totalSize - totalSent, &sent);
            if (error != IDEVICE_E_SUCCESS || sent == 0) {
                qCriticalEx() << "发送失败" << magic_enum::enum_name(error) << "总大小:" << totalSize << "本次发送:" << sent;
                usbConnection = nullptr;
                queuedUsbWrites.prepend(byteArray.mid(totalSent));
                return false;
            }
            totalSent += sent;
        }
        return true;
    }
#endif

    QTcpSocket *tcpSocket = nullptr;
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    idevice_connection_t usbConnection = nullptr;
    QList<QByteArray> queuedUsbWrites;
    QList<QByteArray> queuedUsbRawInputs;
#endif
};