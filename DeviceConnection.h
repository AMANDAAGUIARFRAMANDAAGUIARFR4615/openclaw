#pragma once

#include "Logger.h"
#include "DeviceInfo.h"
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

    explicit DeviceConnection(QTcpSocket *socket) : type(Tcp), tcpSocket(socket), usbConnection(nullptr) {
        tcpSocketToDevice[tcpSocket] = this;
    }

    explicit DeviceConnection(idevice_connection_t connection) : type(Usb), tcpSocket(nullptr), usbConnection(connection) {
        usbConnectionToDevice[usbConnection] = this;
    }

    ~DeviceConnection() {
        emit aboutToDestroyed(this);

        if (type == Tcp)
        {
            tcpSocket->close();
            tcpSocket->deleteLater();
        }
    }

    static DeviceConnection* find(QTcpSocket* socket) {
        return tcpSocketToDevice.value(socket, nullptr);
    }

    static DeviceConnection* find(idevice_connection_t connection) {
        return usbConnectionToDevice.value(connection, nullptr);
    }

    void send(const QString& event, const QJsonValue &jsonValue = QJsonValue()) {
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

        QByteArray jsonData = QJsonDocument(jsonObject).toJson();

        quint32 jsonDataLength = jsonData.size();

        QByteArray dataToSend;
        dataToSend.append(reinterpret_cast<const char*>(&identifier), sizeof(identifier));
        dataToSend.append(reinterpret_cast<const char*>(&jsonDataLength), sizeof(jsonDataLength));
        dataToSend.append(jsonData);

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

    const Type type;

    DeviceInfo* deviceInfo = nullptr;

    QString displayName() {
        return QString("%1 - %2").arg(deviceInfo->deviceName).arg(type == DeviceConnection::Usb ? "USB" : "WIFI");
    }

signals:
    void aboutToDestroyed(DeviceConnection* );

protected:
    QTcpSocket *tcpSocket;
    idevice_connection_t usbConnection;

    inline static QHash<QTcpSocket*, DeviceConnection*> tcpSocketToDevice;
    inline static QHash<idevice_connection_t, DeviceConnection*> usbConnectionToDevice;
};
