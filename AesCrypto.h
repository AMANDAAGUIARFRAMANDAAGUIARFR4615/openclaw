#include <QByteArray>
#include <QString>
#include <QDebug>
#include <QLibrary>
#include <QRandomGenerator>

class AesCrypto {
private:
    static const int EVP_CTRL_GCM_SET_IVLEN = 0x9;
    static const int EVP_CTRL_GCM_GET_TAG   = 0x10;
    static const int EVP_CTRL_GCM_SET_TAG   = 0x11;

    typedef void* (*Func_EVP_CIPHER_CTX_new)();
    typedef void (*Func_EVP_CIPHER_CTX_free)(void*);
    typedef const void* (*Func_EVP_aes_128_gcm)(); // Changed to 128
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

    static bool initOpenSSL() {
        if (m_loaded) return true;
        static QLibrary lib;
        if (lib.isLoaded()) { m_loaded = true; return true; }

        const QStringList libNames = { "libcrypto-3-x64", "libcrypto-1_1-x64", "libcrypto", "crypto" };
        for (const QString &name : libNames) {
            lib.setFileName(name);
            if (lib.load()) break;
        }

        if (!lib.isLoaded()) return false;

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

public:
    static QByteArray encrypt(const QByteArray &plainText, const QByteArray &key) {
        if (!initOpenSSL()) return QByteArray();
        if (key.size() != 16) return QByteArray(); // AES-128 需要 16 字节 Key

        QByteArray iv(12, 0); // GCM 标准 IV 长度 12
        QRandomGenerator::securelySeeded().fillRange(reinterpret_cast<quint32*>(iv.data()), 3);

        void* ctx = ctx_new();
        if (!ctx) return QByteArray();

        if (encrypt_init(ctx, aes_128_gcm(), nullptr, nullptr, nullptr) != 1) { ctx_free(ctx); return QByteArray(); }
        ctx_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
        if (encrypt_init(ctx, nullptr, nullptr, (const unsigned char*)key.data(), (const unsigned char*)iv.data()) != 1) { ctx_free(ctx); return QByteArray(); }

        QByteArray cipherBody(plainText.size(), 0);
        int len = 0, cipherLen = 0;

        encrypt_update(ctx, (unsigned char*)cipherBody.data(), &len, (const unsigned char*)plainText.data(), plainText.size());
        cipherLen = len;
        encrypt_final(ctx, (unsigned char*)cipherBody.data() + len, &len);
        cipherLen += len;

        QByteArray tag(16, 0);
        ctx_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());
        ctx_free(ctx);

        return iv + cipherBody + tag;
    }

    static QByteArray decrypt(const QByteArray &encryptedData, const QByteArray &key) {
        if (!initOpenSSL()) return QByteArray();
        if (key.size() != 16) return QByteArray();
        if (encryptedData.size() < 28) return QByteArray(); // IV(12) + Tag(16)

        QByteArray iv = encryptedData.left(12);
        QByteArray tag = encryptedData.right(16);
        QByteArray cipherBody = encryptedData.mid(12, encryptedData.size() - 28);

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
            return QByteArray(); // 验证失败
        }
        plainLen += len;
        ctx_free(ctx);

        plainText.resize(plainLen);
        return plainText;
    }
};