#pragma once

#include "Logger.h"
#include <QIODevice>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>
#include <QTcpSocket>
#include <QDateTime>

class LiveStreamDevice : public QIODevice {
    Q_OBJECT
public:
    explicit LiveStreamDevice(const QString &hostName = nullptr, quint16 port = 0, QObject *parent = nullptr)
        : QIODevice(parent) {
        if (!port)
            return;

        m_socket = new QTcpSocket();
        connect(m_socket, &QTcpSocket::errorOccurred, [=](QAbstractSocket::SocketError socketError) {
            qCriticalEx() << "errorOccurred" << socketError << m_socket->errorString();
        });
        
        connect(m_socket, &QTcpSocket::readyRead, [=]() {
            appendData(m_socket->readAll());
        });

        m_socket->connectToHost(hostName, port);
    }

    ~LiveStreamDevice() {
        QMutexLocker locker(&m_mutex);
        m_stopped = true;
        m_dataAvailable.wakeAll();

        if (m_socket) {
            m_socket->disconnectFromHost();
            m_socket->deleteLater();
        }
    }

    bool isSequential() const override { return true; }

    void appendData(const QByteArray &data) {
        QMutexLocker locker(&m_mutex);
        m_buffer.append(data);

        quint64 sec = QDateTime::currentSecsSinceEpoch();
        if (sec != m_lastSec) {
            m_prevSecBytes = m_curSecBytes;
            m_curSecBytes = 0;
            m_lastSec = sec;
        }
        m_curSecBytes += data.size();

        m_dataAvailable.wakeAll();
    }

    qint64 speedBps() const {
        QMutexLocker locker(&m_mutex);
        return m_prevSecBytes;
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        QMutexLocker locker(&m_mutex);

        while (m_buffer.isEmpty() && !m_stopped) {
            m_dataAvailable.wait(&m_mutex);
        }

        if (m_stopped)
            return -1;

        qint64 bytesToRead = qMin(maxSize, qint64(m_buffer.size()));
        memcpy(data, m_buffer.constData(), bytesToRead);
        m_buffer.remove(0, bytesToRead);
        return bytesToRead;
    }

    qint64 writeData(const char *, qint64) override {
        return -1;
    }

    bool atEnd() const override {
        return false;
    }

    QTcpSocket* m_socket = nullptr;
    QByteArray m_buffer;
    mutable QMutex m_mutex;
    QWaitCondition m_dataAvailable;
    qint64 m_curSecBytes = 0;
    qint64 m_prevSecBytes = 0;
    quint64 m_lastSec = 0;
    bool m_stopped = false;
};
