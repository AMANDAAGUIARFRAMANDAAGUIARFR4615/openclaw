#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QRandomGenerator>
#include <QByteArrayView>
#include <QIODevice>
#include <QApplication>

template <typename T>
class SafeObject {
public:
    SafeObject(const T& value = T()) {
        set(value);
    }

    void set(const T& value) {
        // 1. 序列化对象到字节数组
        QByteArray rawData;
        QDataStream stream(&rawData, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);
        stream << value;

        // 2. 生成随机 Key
        m_key = QByteArray(rawData.size(), Qt::Uninitialized);
        QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(m_key.data()), m_key.size() / 4);

        // 3. 计算原始数据的校验和
        m_checksum = qChecksum(QByteArrayView(rawData));

        // 4. 加密
        m_encryptedData = xorBytes(rawData, m_key);
    }

    T get() const {
        // 1. 解密
        QByteArray decryptedData = xorBytes(m_encryptedData, m_key);

        // 2. 完整性检测
        quint16 currentChecksum = qChecksum(QByteArrayView(decryptedData));
        if (currentChecksum != m_checksum) {
            // qCritical() << "[ALERT] Memory tampering detected! Data has been modified externally.";
            *(int*)qApp = 0;
        }

        // 3. 反序列化回对象
        T value;
        QDataStream stream(&decryptedData, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_10);
        stream >> value;

        return value;
    }

private:
    QByteArray m_encryptedData;
    QByteArray m_key;
    quint16 m_checksum;

    QByteArray xorBytes(const QByteArray& data, const QByteArray& key) const {
        QByteArray result = data;
        for (int i = 0; i < result.size(); ++i) {
            result[i] = result[i] ^ key[i % key.size()];
        }
        return result;
    }
};
