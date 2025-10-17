#include "DeviceConnection.h"

QHash<QTcpSocket*, DeviceConnection*> DeviceConnection::tcpSocketToDevice;
QHash<idevice_connection_t, DeviceConnection*> DeviceConnection::usbConnectionToDevice;

DeviceConnection::DeviceConnection(QTcpSocket *socket)
    : type(Tcp), tcpSocket(socket), usbConnection(nullptr)
{
    tcpSocketToDevice[tcpSocket] = this;
}

DeviceConnection::DeviceConnection(idevice_connection_t connection)
    : type(Usb), tcpSocket(nullptr), usbConnection(connection)
{
    usbConnectionToDevice[usbConnection] = this;
}

DeviceConnection* DeviceConnection::find(QTcpSocket* socket)
{
    return tcpSocketToDevice.value(socket, nullptr);
}

DeviceConnection* DeviceConnection::find(idevice_connection_t connection)
{
    return usbConnectionToDevice.value(connection, nullptr);
}

void DeviceConnection::send(const QString& event, const QJsonValue &jsonValue)
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

bool DeviceConnection::operator==(const DeviceConnection &other) const
{
    if (type != other.type)
        return false;

    if (type == Tcp)
        return tcpSocket == other.tcpSocket;

    return usbConnection == other.usbConnection;
}

bool DeviceConnection::operator!=(const DeviceConnection& other) const {
    return !(*this == other);
}
