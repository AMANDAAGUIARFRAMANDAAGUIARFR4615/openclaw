#pragma once

#include "Logger.h"
#include "DeviceInfo.h"
#include <QByteArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <libimobiledevice/libimobiledevice.h>
#include <QHash>

class DeviceConnection
{
public:
    enum Type {
        Usb,
        Tcp
    };

    explicit DeviceConnection(QTcpSocket *socket);
    explicit DeviceConnection(idevice_connection_t connection);

    static DeviceConnection* find(QTcpSocket* socket);
    static DeviceConnection* find(idevice_connection_t connection);

    void send(const QString& event, const QJsonValue &jsonValue = QJsonValue());
    void write(const QByteArray &byteArray);
    void close();

    bool operator==(const DeviceConnection& other) const;
    bool operator!=(const DeviceConnection& other) const;

    const Type type;

    DeviceInfo* deviceInfo;

    QString displayName() {
        return QString("%1 - %2").arg(deviceInfo->deviceName).arg(type == DeviceConnection::Usb ? "USB" : "WIFI");
    }

private:
    QTcpSocket *tcpSocket;
    idevice_connection_t usbConnection;

    static QHash<QTcpSocket*, DeviceConnection*> tcpSocketToDevice;
    static QHash<idevice_connection_t, DeviceConnection*> usbConnectionToDevice;
};
