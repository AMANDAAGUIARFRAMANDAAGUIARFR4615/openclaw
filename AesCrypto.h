#include <QByteArray>
#include <QString>
#include <QDebug>
#include <QLibrary>
#include <QRandomGenerator>

class AesCrypto {
private:
    // --- 定义函数指针类型 ---
    typedef void* (*Func_EVP_CIPHER_CTX_new)();
    typedef void (*Func_EVP_CIPHER_CTX_free)(void*);
    typedef const void* (*Func_EVP_aes_256_cbc)();
    typedef int (*Func_EVP_EncryptInit_ex)(void*, const void*, void*, const unsigned char*, const unsigned char*);
    typedef int (*Func_EVP_EncryptUpdate)(void*, unsigned char*, int*, const unsigned char*, int);
    typedef int (*Func_EVP_EncryptFinal_ex)(void*, unsigned char*, int*);
    typedef int (*Func_EVP_DecryptInit_ex)(void*, const void*, void*, const unsigned char*, const unsigned char*);
    typedef int (*Func_EVP_DecryptUpdate)(void*, unsigned char*, int*, const unsigned char*, int);
    typedef int (*Func_EVP_DecryptFinal_ex)(void*, unsigned char*, int*);

    // --- 静态成员变量 (C++17) ---
    static inline Func_EVP_CIPHER_CTX_new     ctx_new = nullptr;
    static inline Func_EVP_CIPHER_CTX_free    ctx_free = nullptr;
    static inline Func_EVP_aes_256_cbc        aes_256_cbc = nullptr;
    
    static inline Func_EVP_EncryptInit_ex     encrypt_init = nullptr;
    static inline Func_EVP_EncryptUpdate      encrypt_update = nullptr;
    static inline Func_EVP_EncryptFinal_ex    encrypt_final = nullptr;
    
    static inline Func_EVP_DecryptInit_ex     decrypt_init = nullptr;
    static inline Func_EVP_DecryptUpdate      decrypt_update = nullptr;
    static inline Func_EVP_DecryptFinal_ex    decrypt_final = nullptr;

    static inline bool m_loaded = false;

    // --- 初始化加载 OpenSSL ---
    static bool initOpenSSL() {
        if (m_loaded) return true;

        // 静态变量保证库句柄在程序运行期间一直有效
        static QLibrary lib;
        if (lib.isLoaded()) {
            m_loaded = true;
            return true;
        }

        // 常用库名列表
        const QStringList libNames = {
            "libcrypto-3-x64",   // Windows OpenSSL 3
            "libcrypto-1_1-x64", // Windows OpenSSL 1.1
            "libcrypto",         // Linux
            "crypto"             // macOS
        };

        for (const QString &name : libNames) {
            lib.setFileName(name);
            if (lib.load()) {
                break;
            }
        }

        if (!lib.isLoaded()) {
            qWarning() << "AesCrypto: Failed to load OpenSSL library.";
            return false;
        }

        // 解析函数地址
        ctx_new = (Func_EVP_CIPHER_CTX_new)lib.resolve("EVP_CIPHER_CTX_new");
        ctx_free = (Func_EVP_CIPHER_CTX_free)lib.resolve("EVP_CIPHER_CTX_free");
        aes_256_cbc = (Func_EVP_aes_256_cbc)lib.resolve("EVP_aes_256_cbc");
        
        encrypt_init = (Func_EVP_EncryptInit_ex)lib.resolve("EVP_EncryptInit_ex");
        encrypt_update = (Func_EVP_EncryptUpdate)lib.resolve("EVP_EncryptUpdate");
        encrypt_final = (Func_EVP_EncryptFinal_ex)lib.resolve("EVP_EncryptFinal_ex");
        
        decrypt_init = (Func_EVP_DecryptInit_ex)lib.resolve("EVP_DecryptInit_ex");
        decrypt_update = (Func_EVP_DecryptUpdate)lib.resolve("EVP_DecryptUpdate");
        decrypt_final = (Func_EVP_DecryptFinal_ex)lib.resolve("EVP_DecryptFinal_ex");

        // 验证完整性
        if (ctx_new && ctx_free && aes_256_cbc && 
            encrypt_init && encrypt_update && encrypt_final &&
            decrypt_init && decrypt_update && decrypt_final) {
            m_loaded = true;
        } else {
            qWarning() << "AesCrypto: Failed to resolve symbols.";
            m_loaded = false;
        }

        return m_loaded;
    }

public:
    static QByteArray encrypt(const QByteArray &plainText, const QByteArray &key) {
        if (!initOpenSSL()) return QByteArray();
        if (key.size() != 32) return QByteArray(); // AES-256 需要 32字节 Key

        // 1. 生成随机 IV (16字节)
        QByteArray iv(16, 0);
        QRandomGenerator::securelySeeded().fillRange(
            reinterpret_cast<quint32*>(iv.data()), iv.size() / 4);

        void* ctx = ctx_new();
        if (!ctx) return QByteArray();

        // 2. 初始化加密
        if (1 != encrypt_init(ctx, aes_256_cbc(), nullptr, 
                             (const unsigned char*)key.data(), 
                             (const unsigned char*)iv.data())) {
            ctx_free(ctx);
            return QByteArray();
        }

        QByteArray cipherBody;
        cipherBody.resize(plainText.size() + 16); // 预留 Block 空间

        int len = 0;
        int cipherLen = 0;

        // 3. 加密数据
        encrypt_update(ctx, (unsigned char*)cipherBody.data(), &len, 
                       (const unsigned char*)plainText.data(), plainText.size());
        cipherLen = len;

        // 4. 结束加密 (处理 Padding)
        encrypt_final(ctx, (unsigned char*)cipherBody.data() + len, &len);
        cipherLen += len;

        ctx_free(ctx);
        cipherBody.resize(cipherLen);

        // 返回: IV + 密文
        return iv + cipherBody;
    }

    static QByteArray decrypt(const QByteArray &encryptedData, const QByteArray &key) {
        if (!initOpenSSL()) return QByteArray();
        if (key.size() != 32) return QByteArray();
        if (encryptedData.size() < 16) return QByteArray(); // 至少要有 IV

        // 1. 提取 IV 和 密文主体
        QByteArray iv = encryptedData.left(16);
        QByteArray cipherBody = encryptedData.mid(16);

        void* ctx = ctx_new();
        if (!ctx) return QByteArray();

        // 2. 初始化解密
        if (1 != decrypt_init(ctx, aes_256_cbc(), nullptr, 
                             (const unsigned char*)key.data(), 
                             (const unsigned char*)iv.data())) {
            ctx_free(ctx);
            return QByteArray();
        }

        QByteArray plainText;
        plainText.resize(cipherBody.size() + 16);

        int len = 0;
        int plainLen = 0;

        // 3. 解密数据
        decrypt_update(ctx, (unsigned char*)plainText.data(), &len, 
                       (const unsigned char*)cipherBody.data(), cipherBody.size());
        plainLen = len;

        // 4. 结束解密 (验证 Padding)
        if (1 != decrypt_final(ctx, (unsigned char*)plainText.data() + len, &len)) {
            ctx_free(ctx); // 解密失败 (通常是 Key 错误或数据损坏)
            return QByteArray();
        }
        plainLen += len;

        ctx_free(ctx);
        plainText.resize(plainLen);

        return plainText;
    }
};