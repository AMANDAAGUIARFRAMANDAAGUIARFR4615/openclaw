#pragma once

#include "Logger.h"
#include <QIODevice>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>
#include <QTcpSocket>

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
        m_dataAvailable.wakeAll();
    }

    void endStream() {
        QMutexLocker locker(&m_mutex);
        m_eof = true;
        m_dataAvailable.wakeAll();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        QMutexLocker locker(&m_mutex);

        // 若无数据且未结束，等待
        while (m_buffer.isEmpty() && !m_eof) {
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
};
