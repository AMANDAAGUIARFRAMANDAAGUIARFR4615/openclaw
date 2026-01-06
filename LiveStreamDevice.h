#ifndef LIVESTREAMDEVICE_H
#define LIVESTREAMDEVICE_H

#include <QIODevice>
#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>

// 类名改为 LiveStreamDevice，更加贴切
class LiveStreamDevice : public QIODevice
{
    Q_OBJECT
public:
    explicit LiveStreamDevice(QObject *parent = nullptr) : QIODevice(parent)
    {
        // 初始化时必须打开设备，设为读写模式
        open(QIODevice::ReadWrite);
    }

    // ---------------------------------------------------------
    // 供 Socket 调用的核心函数：写入数据
    // ---------------------------------------------------------
    void appendData(const QByteArray &data)
    {
        if (data.isEmpty()) return;

        {
            QMutexLocker locker(&m_mutex);
            m_buffer.append(data);
        } // 锁在这里释放，避免 emit 信号时死锁

        // 极其重要：告诉 QMediaPlayer 有新数据到了，赶紧来读
        emit readyRead();
    }

    // ---------------------------------------------------------
    // QIODevice 必须重写的虚函数
    // ---------------------------------------------------------

    // QMediaPlayer 通过此函数读取数据
    qint64 readData(char *data, qint64 maxlen) override
    {
        QMutexLocker locker(&m_mutex);
        
        qint64 len = qMin((qint64)m_buffer.size(), maxlen);
        
        if (len > 0) {
            // 将内部 buffer 的数据拷贝到播放器提供的指针中
            memcpy(data, m_buffer.constData(), len);
            // 移除已读取的数据，像队列一样先进先出
            m_buffer.remove(0, len);
        }
        
        return len;
    }

    // 这里通常用不到 writeData，因为数据来源是 appendData
    // 但作为 QIODevice，还是实现一下标准接口
    qint64 writeData(const char *data, qint64 len) override
    {
        QByteArray tmp(data, len);
        appendData(tmp);
        return len;
    }

    // 告诉播放器当前有多少数据可读
    qint64 bytesAvailable() const override
    {
        QMutexLocker locker(&m_mutex);
        return m_buffer.size() + QIODevice::bytesAvailable();
    }

    // 声明这是顺序流（不可 seek，不可倒带）
    bool isSequential() const override
    {
        return true;
    }

    bool atEnd() const override {
        return false;
    }

private:
    QByteArray m_buffer;
    mutable QMutex m_mutex; // 加上锁，因为播放器的读取可能在独立线程
};

#endif // LIVESTREAMDEVICE_H