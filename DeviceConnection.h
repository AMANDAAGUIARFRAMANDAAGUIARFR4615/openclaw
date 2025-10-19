#pragma once

#include "Logger.h"
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

    bool operator==(const DeviceConnection &other) const;
    bool operator!=(const DeviceConnection& other) const;

    const Type type;

private:
    QTcpSocket *tcpSocket;
    idevice_connection_t usbConnection;

    static QHash<QTcpSocket*, DeviceConnection*> tcpSocketToDevice;
    static QHash<idevice_connection_t, DeviceConnection*> usbConnectionToDevice;
};
