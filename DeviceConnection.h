#pragma once

#include "Logger.h"
#include <QByteArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <libimobiledevice/libimobiledevice.h>

class DeviceConnection
{
public:
    enum Type {
        Usb,
        Tcp
    };

    explicit DeviceConnection(QTcpSocket *socket) : type(Tcp), tcpSocket(socket), usbConnection(nullptr) {}

    explicit DeviceConnection(idevice_connection_t connection) : type(Usb), tcpSocket(nullptr), usbConnection(connection) {}

    void send(const QString& event, const QJsonValue &jsonValue = QJsonValue())
    {
        QJsonObject jsonObject;
        jsonObject["event"] = event;

        if (!jsonValue.isUndefined() && !jsonValue.isNull())
            jsonObject["data"] = jsonValue;

        qDebugEx() << "sendData" << jsonObject;

        if (type == Tcp && tcpSocket->state() != QAbstractSocket::ConnectedState) {
            qDebugEx() << "不是连接状态，无法发送数据";
            return;
        }

        quint64 identifier = 0xc6e8f3de9a654d6b;

        QJsonDocument doc(jsonObject);
        QByteArray jsonData = doc.toJson();

        quint32 jsonDataLength = jsonData.size();

        QByteArray dataToSend;
        dataToSend.append(reinterpret_cast<const char*>(&identifier), sizeof(identifier));
        dataToSend.append(reinterpret_cast<const char*>(&jsonDataLength), sizeof(jsonDataLength));
        dataToSend.append(jsonData);

        if (type == Tcp)
        {
            tcpSocket->write(dataToSend);
            tcpSocket->flush();
        }
        else
        {
            uint32_t sent = 0;
            idevice_error_t error = idevice_connection_send(usbConnection, dataToSend.constData(), dataToSend.size(), &sent);
            if (error != IDEVICE_E_SUCCESS)
                qDebugEx() << "发送失败" << error;
        }
    }

    inline bool operator==(const DeviceConnection &other) const
    {
        if (type != other.type)
            return false;

        if (type == Tcp)
            return tcpSocket == other.tcpSocket;

        return usbConnection == other.usbConnection;
    }

private:
    Type type;
    QTcpSocket *tcpSocket;
    idevice_connection_t usbConnection;
};
