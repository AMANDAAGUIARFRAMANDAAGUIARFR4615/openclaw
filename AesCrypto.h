#include "Logger.h"

#include <QByteArray>
#include <QString>
#include <QDebug>
#include <QLibrary>
#include <QRandomGenerator>
#include <QVector>

class AesCrypto {
private:
    static const int EVP_CTRL_GCM_SET_IVLEN = 0x9;
    static const int EVP_CTRL_GCM_GET_TAG   = 0x10;
    static const int EVP_CTRL_GCM_SET_TAG   = 0x11;

    typedef void* (*Func_EVP_CIPHER_CTX_new)();
    typedef void (*Func_EVP_CIPHER_CTX_free)(void*);
    typedef const void* (*Func_EVP_aes_128_gcm)();
    typedef int (*Func_EVP_CIPHER_CTX_ctrl)(void*, int, int, void*);
    typedef int (*Func_EVP_EncryptInit_ex)(void*, const void*, void*, const unsigned char*, const unsigned char*);
    typedef int (*Func_EVP_EncryptUpdate)(void*, unsigned char*, int*, const unsigned char*, int);
    typedef int (*Func_EVP_EncryptFinal_ex)(void*, unsigned char*, int*);
    typedef int (*Func_EVP_DecryptInit_ex)(void*, const void*, void*, const unsigned char*, const unsigned char*);
    typedef int (*Func_EVP_DecryptUpdate)(void*, unsigned char*, int*, const unsigned char*, int);
    typedef int (*Func_EVP_DecryptFinal_ex)(void*, unsigned char*, int*);

    static inline Func_EVP_CIPHER_CTX_new     ctx_new = nullptr;
    static inline Func_EVP_CIPHER_CTX_free    ctx_free = nullptr;
    static inline Func_EVP_aes_128_gcm        aes_128_gcm = nullptr;
    static inline Func_EVP_CIPHER_CTX_ctrl    ctx_ctrl = nullptr;
    static inline Func_EVP_EncryptInit_ex     encrypt_init = nullptr;
    static inline Func_EVP_EncryptUpdate      encrypt_update = nullptr;
    static inline Func_EVP_EncryptFinal_ex    encrypt_final = nullptr;
    static inline Func_EVP_DecryptInit_ex     decrypt_init = nullptr;
    static inline Func_EVP_DecryptUpdate      decrypt_update = nullptr;
    static inline Func_EVP_DecryptFinal_ex    decrypt_final = nullptr;

    static inline bool m_loaded = false;
    static inline QVector<QByteArray> m_keyTable;

    static const quint32 RANDOM_SEED = 0x6D2B79F5; // 随机种子
    static const int KEY_COUNT = 4096;             // 密钥数量
    static const quint16 OBFUSCATE_MASK = 0xA55A;  // 混淆掩码

    static bool initOpenSSL() {
        if (m_loaded) return true;

        static QLibrary lib;

#ifdef Q_OS_WIN
        lib.setFileName("libcrypto-3-x64.dll");
#else
        QString libName = "libcrypto.3.dylib";
        QString frameworksDir = QCoreApplication::applicationDirPath() + "/../Frameworks/";
        if (QDir(frameworksDir).exists())
            lib.setFileName(frameworksDir + libName);
        else
            lib.setFileName(QString("/opt/homebrew/opt/openssl/lib/") + libName);
#endif

        lib.load();

        if (!lib.isLoaded()) return false;

        qDebugEx() << "libcrypto加载成功" << lib.fileName();

        ctx_new = (Func_EVP_CIPHER_CTX_new)lib.resolve("EVP_CIPHER_CTX_new");
        ctx_free = (Func_EVP_CIPHER_CTX_free)lib.resolve("EVP_CIPHER_CTX_free");
        aes_128_gcm = (Func_EVP_aes_128_gcm)lib.resolve("EVP_aes_128_gcm");
        ctx_ctrl = (Func_EVP_CIPHER_CTX_ctrl)lib.resolve("EVP_CIPHER_CTX_ctrl");
        encrypt_init = (Func_EVP_EncryptInit_ex)lib.resolve("EVP_EncryptInit_ex");
        encrypt_update = (Func_EVP_EncryptUpdate)lib.resolve("EVP_EncryptUpdate");
        encrypt_final = (Func_EVP_EncryptFinal_ex)lib.resolve("EVP_EncryptFinal_ex");
        decrypt_init = (Func_EVP_DecryptInit_ex)lib.resolve("EVP_DecryptInit_ex");
        decrypt_update = (Func_EVP_DecryptUpdate)lib.resolve("EVP_DecryptUpdate");
        decrypt_final = (Func_EVP_DecryptFinal_ex)lib.resolve("EVP_DecryptFinal_ex");

        if (ctx_new && ctx_free && aes_128_gcm && ctx_ctrl &&
            encrypt_init && encrypt_update && encrypt_final &&
            decrypt_init && decrypt_update && decrypt_final) {
            m_loaded = true;
        }
        return m_loaded;
    }

    // -----------------------------------------------------------
    // LCG 伪随机数算法
    // -----------------------------------------------------------
    static inline quint32 m_seedState = 0;

    static void my_srand(quint32 seed) {
        m_seedState = seed;
    }

    static int my_rand() {
        m_seedState = m_seedState * 214013 + 2531011;
        return (m_seedState >> 16) & 0x7FFF;
    }

    static void initKeys() {
        if (!m_keyTable.isEmpty()) return;

        my_srand(RANDOM_SEED);

        m_keyTable.reserve(KEY_COUNT);
        for (int i = 0; i < KEY_COUNT; ++i) {
            QByteArray key(16, 0);
            unsigned char* p = (unsigned char*)key.data();
            for(int j = 0; j < 16; ++j) {
                p[j] = (unsigned char)(my_rand() & 0xFF);
            }
            m_keyTable.append(key);
        }
    }

public:
    static QByteArray encrypt(const QByteArray &plainText) {
        if (!initOpenSSL()) return QByteArray();
        
        initKeys();

        quint16 keyIndex = (quint16)QRandomGenerator::global()->bounded(KEY_COUNT);
        QByteArray key = m_keyTable[keyIndex];

        QByteArray iv(12, 0);
        QRandomGenerator::securelySeeded().fillRange(reinterpret_cast<quint32*>(iv.data()), 3);

        quint16 ivPart = *reinterpret_cast<const quint16*>(iv.data());
        
        quint16 storedValue = (keyIndex ^ OBFUSCATE_MASK) + ivPart;

        void* ctx = ctx_new();
        if (!ctx) return QByteArray();

        if (encrypt_init(ctx, aes_128_gcm(), nullptr, nullptr, nullptr) != 1) { ctx_free(ctx); return QByteArray(); }
        ctx_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
        if (encrypt_init(ctx, nullptr, nullptr, (const unsigned char*)key.data(), (const unsigned char*)iv.data()) != 1) { ctx_free(ctx); return QByteArray(); }

        QByteArray cipherBody(plainText.size(), 0);
        int len = 0;
        encrypt_update(ctx, (unsigned char*)cipherBody.data(), &len, (const unsigned char*)plainText.data(), plainText.size());
        int finalLen = 0;
        encrypt_final(ctx, (unsigned char*)cipherBody.data() + len, &finalLen);

        QByteArray tag(16, 0);
        ctx_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());
        ctx_free(ctx);

        QByteArray result;
        result.append(reinterpret_cast<const char*>(&storedValue), sizeof(storedValue));
        result.append(iv);
        result.append(cipherBody);
        result.append(tag);
        return result;
    }

    static QByteArray decrypt(const QByteArray &encryptedData) {
        if (!initOpenSSL()) return QByteArray();

        initKeys();

        // 校验长度：2(Index) + 12(IV) + 16(Tag) = 30
        if (encryptedData.size() < 30) return QByteArray();

        quint16 storedValue = *reinterpret_cast<const quint16*>(encryptedData.data());

        QByteArray iv = encryptedData.mid(2, 12);
        quint16 ivPart = *reinterpret_cast<const quint16*>(iv.data());

        quint16 keyIndex = (storedValue - ivPart) ^ OBFUSCATE_MASK;

        // 索引越界，说明数据损坏或被篡改
        if (keyIndex >= m_keyTable.size())
            return QByteArray();
        
        QByteArray key = m_keyTable[keyIndex];

        QByteArray tag = encryptedData.right(16);
        QByteArray cipherBody = encryptedData.mid(14, encryptedData.size() - 30);

        void* ctx = ctx_new();
        if (!ctx) return QByteArray();

        if (decrypt_init(ctx, aes_128_gcm(), nullptr, nullptr, nullptr) != 1) { ctx_free(ctx); return QByteArray(); }
        ctx_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
        if (decrypt_init(ctx, nullptr, nullptr, (const unsigned char*)key.data(), (const unsigned char*)iv.data()) != 1) { ctx_free(ctx); return QByteArray(); }

        QByteArray plainText(cipherBody.size(), 0);
        int len = 0, plainLen = 0;
        decrypt_update(ctx, (unsigned char*)plainText.data(), &len, (const unsigned char*)cipherBody.data(), cipherBody.size());
        plainLen = len;

        ctx_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag.data());
        if (decrypt_final(ctx, (unsigned char*)plainText.data() + len, &len) != 1) {
            ctx_free(ctx);
            return QByteArray(); 
        }
        plainLen += len;
        ctx_free(ctx);

        plainText.resize(plainLen);
        return plainText;
    }
};
