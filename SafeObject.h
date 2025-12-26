#include <QByteArray>
#include <QDataStream>
#include <QRandomGenerator>
#include <QByteArrayView>
#include <QIODevice>
#include <QApplication>

template <typename T>
class SafeObject {
public:
    SafeObject(const T& value) {
        set(value);
    }

    void set(const T& value) {
        // 1. 序列化对象到字节数组
        QByteArray rawData;
        QDataStream stream(&rawData, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);
        stream << value;

        // 2. 生成随机 Key (长度与数据一致，类似于一次性密码本)
        m_key = QByteArray(rawData.size(), Qt::Uninitialized);
        QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(m_key.data()), m_key.size() / 4);

        // 3. 计算原始数据的校验和 (用于检测篡改)
        // 使用 CRC-16 (qChecksum) 比较轻量，也可以用 MD5
        m_checksum = qChecksum(QByteArrayView(rawData));

        // 4. 加密 (异或运算)
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

    // --- 模拟内存被修改 ---
    void simulateHack() {
        if (!m_encryptedData.isEmpty()) {
            // 攻击者通常通过内存搜索修改字节，这里我们强行修改加密数据的一个字节
            // 由于攻击者不知道 Key，他修改后的数据解密出来肯定无法通过校验
            m_encryptedData[0] = ~m_encryptedData[0];
            qDebug() << ">> Simulating external modification (HACKING)...";
        }
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
