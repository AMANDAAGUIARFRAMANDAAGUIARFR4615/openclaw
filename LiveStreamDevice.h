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

        auto socket = new QTcpSocket();
        connect(socket, &QTcpSocket::errorOccurred, this, [=](QAbstractSocket::SocketError socketError) {
            qCriticalEx() << "errorOccurred" << socketError << socket->errorString();
        });
        
        connect(socket, &QTcpSocket::readyRead, this, [=]() {
            appendData(socket->readAll());
        });

        socket->connectToHost(hostName, port);
    }

    bool isSequential() const override { return true; }

    bool open(OpenMode mode) override {
        if (!(mode & ReadOnly))
            return false;

        m_eof = false;
        m_firstThresholdReached = false;
        return QIODevice::open(mode);
    }

    void close() override {
        QMutexLocker locker(&m_mutex);
        m_buffer.clear();
        m_eof = true;
        m_dataAvailable.wakeAll();
        QIODevice::close();
    }

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
        if (m_firstThresholdReached || m_buffer.size() >= 2048) {
            m_firstThresholdReached = true;
            m_dataAvailable.wakeAll();
        }
    }

    qint64 speedBps() const {
        QMutexLocker locker(&m_mutex);
        return m_prevSecBytes;
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        QMutexLocker locker(&m_mutex);

        while ((m_buffer.isEmpty() || !m_firstThresholdReached) && !m_eof) {
            m_dataAvailable.wait(&m_mutex);
        }

        if (m_buffer.isEmpty() && m_eof)
            return 0; // EOF

        qint64 bytesToRead = qMin(maxSize, qint64(m_buffer.size()));
        memcpy(data, m_buffer.constData(), bytesToRead);
        m_buffer.remove(0, bytesToRead);
        return bytesToRead;
    }

    qint64 writeData(const char *, qint64) override {
        // 不支持直接写入
        return -1;
    }

    bool atEnd() const override {
        QMutexLocker locker(&m_mutex);
        return m_eof && m_buffer.isEmpty();
    }

    QByteArray m_buffer;
    mutable QMutex m_mutex;
    QWaitCondition m_dataAvailable;
    bool m_eof = false;
    bool m_firstThresholdReached = false;
    qint64 m_curSecBytes = 0;
    qint64 m_prevSecBytes = 0;
    quint64 m_lastSec = 0;
};