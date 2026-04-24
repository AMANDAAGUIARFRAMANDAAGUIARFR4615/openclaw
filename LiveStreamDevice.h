#pragma once

#include <QIODevice>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>
#include <QDateTime>
#include <QSharedPointer>

class LiveStreamDevice : public QIODevice {
    Q_OBJECT

    // 将所有线程共享的数据放入这个内部结构体
    struct InternalData {
        QMutex mutex;
        QWaitCondition dataAvailable;
        QByteArray buffer;
        bool stopped = false;
    };

public:
    explicit LiveStreamDevice(QObject *parent = nullptr) : QIODevice(parent) {
        m_data = QSharedPointer<InternalData>::create();
        open(QIODevice::ReadOnly);
    }

    ~LiveStreamDevice() {
        stop();
    }

    void stop() {
        QMutexLocker locker(&m_data->mutex);
        m_data->stopped = true;
        m_data->dataAvailable.wakeAll();
    }

    bool isSequential() const override { return true; }

    void appendData(const QByteArray &data) {
        QMutexLocker locker(&m_data->mutex);
        if (m_data->stopped)
            return;

        m_data->buffer.append(data);
        m_data->dataAvailable.wakeAll();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        // 【防崩溃的核心】：创建局部 shared_ptr
        // 只要这个 d 变量还在作用域内，InternalData就绝对不会被销毁
        auto d = m_data;
        
        QMutexLocker locker(&d->mutex);

        while (d->buffer.isEmpty() && !d->stopped) {
            // wait 会暂时解开锁。
            // 此时如果主线程销毁了 LiveStreamDevice，m_data 成员变量没了，
            // 但我们的局部变量 d 还在，所以 mutex 依然有效！
            d->dataAvailable.wait(&d->mutex);
        }

        if (d->stopped)
            return -1; // 返回 -1 告诉播放器流结束了

        qint64 bytesToRead = qMin(maxSize, qint64(d->buffer.size()));
        memcpy(data, d->buffer.constData(), bytesToRead);
        d->buffer.remove(0, bytesToRead);
        
        return bytesToRead;
    } // 函数结束，d 销毁。如果外部对象已析构，InternalData 在此处真正释放。

    qint64 writeData(const char *, qint64) override { return -1; }
    
    bool atEnd() const override {
        return false;
    }

private:
    // 唯一的成员变量，管理所有资源的生命周期
    QSharedPointer<InternalData> m_data;
};
