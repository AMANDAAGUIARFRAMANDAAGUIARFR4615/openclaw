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
    explicit LiveStreamDevice(QObject *parent = nullptr) : QIODevice(parent) {}

    ~LiveStreamDevice() {
        QMutexLocker locker(&m_mutex);
        m_stopped = true;
        m_dataAvailable.wakeAll();
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

    QByteArray m_buffer;
    mutable QMutex m_mutex;
    QWaitCondition m_dataAvailable;
    qint64 m_curSecBytes = 0;
    qint64 m_prevSecBytes = 0;
    quint64 m_lastSec = 0;
    bool m_stopped = false;
};
