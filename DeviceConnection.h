#pragma once

#include "DeviceInfo.h"
#include "AesCrypto.h"
#include "UsbDeviceManager.h"
#include <magic_enum/magic_enum.hpp>
#include <QByteArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <libimobiledevice/libimobiledevice.h>
#include <QHash>

class DeviceConnection : public QObject
{
    Q_OBJECT

public:
    enum Type {
        Usb,
        Tcp
    };

    explicit DeviceConnection(QTcpSocket *socket) : type(Tcp), tcpSocket(socket), usbConnection(nullptr), QObject(socket) {}
    explicit DeviceConnection(idevice_connection_t connection) : type(Usb), tcpSocket(nullptr), usbConnection(connection), QObject(nullptr) {}

    void send(const StringGuard::Obfuscator<>& event, const QJsonValue &jsonValue = QJsonValue()) {
        QJsonObject jsonObject;
        jsonObject["event"] = event.decrypt();

        if (!jsonValue.isUndefined() && !jsonValue.isNull())
            jsonObject["data"] = jsonValue;

        qDebugEx() << this << jsonObject;

        if (type == Tcp && tcpSocket->state() != QAbstractSocket::ConnectedState) {
            qDebugEx() << "不是连接状态，无法发送数据";
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
            if (!usbConnection)
                return;

            quint32 sent = 0;
            idevice_error_t error = idevice_connection_send(usbConnection, byteArray.constData(), byteArray.size(), &sent);
            if (error != IDEVICE_E_SUCCESS) {
                qCriticalEx() << "发送失败" << magic_enum::enum_name(error) << byteArray.size() << sent;
                usbConnection = nullptr;
            }
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

    QString displayName(bool richText = false) {
        const auto& palette = qApp->palette();

        const auto& accentColor = palette.color(QPalette::WindowText).name();

        auto shadowQColor = palette.color(QPalette::WindowText);
        shadowQColor.setAlpha(150);
        const auto& shadowColor = shadowQColor.name(QColor::HexArgb);

        const auto& connType = type == DeviceConnection::Usb ? "USB" : "WiFi";
        
        if (richText)
            return QString("<a href='#' style='text-decoration:none; color:%1;'><b>%2</b>✏️</a> <font color='%3'>[%4]</font>").arg(accentColor, deviceInfo->deviceName, shadowColor, connType);
    
        return QString("%1 [%2]").arg(deviceInfo->deviceName, connType);
    }

protected:
    QTcpSocket *tcpSocket;
    idevice_connection_t usbConnection;
};
