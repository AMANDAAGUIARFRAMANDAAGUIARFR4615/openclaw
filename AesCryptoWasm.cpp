#include "AesCrypto.h"
#include <mbedtls/gcm.h>

QByteArray AesCrypto::wasmEncryptBody(quint16 storedValue, const QByteArray &key, const QByteArray &iv,
                                      const QByteArray &plainText)
{
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, reinterpret_cast<const unsigned char *>(key.constData()),
                           128) != 0) {
        mbedtls_gcm_free(&gcm);
        return QByteArray();
    }
    QByteArray cipherBody(plainText.size(), 0);
    QByteArray tag(16, 0);
    const int err = mbedtls_gcm_crypt_and_tag(
        &gcm, MBEDTLS_GCM_ENCRYPT, static_cast<size_t>(plainText.size()),
        reinterpret_cast<const unsigned char *>(iv.constData()), static_cast<size_t>(iv.size()), nullptr, 0,
        reinterpret_cast<const unsigned char *>(plainText.constData()),
        reinterpret_cast<unsigned char *>(cipherBody.data()), tag.size(),
        reinterpret_cast<unsigned char *>(tag.data()));
    mbedtls_gcm_free(&gcm);
    if (err != 0)
        return QByteArray();

    QByteArray result;
    result.append(reinterpret_cast<const char *>(&storedValue), sizeof(storedValue));
    result.append(iv);
    result.append(cipherBody);
    result.append(tag);
    return result;
}

QByteArray AesCrypto::wasmDecryptBody(const QByteArray &key, const QByteArray &iv, const QByteArray &cipherBody,
                                      const QByteArray &tag)
{
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, reinterpret_cast<const unsigned char *>(key.constData()),
                           128) != 0) {
        mbedtls_gcm_free(&gcm);
        return QByteArray();
    }
    QByteArray plainText(cipherBody.size(), 0);
    const int err = mbedtls_gcm_auth_decrypt(
        &gcm, static_cast<size_t>(cipherBody.size()),
        reinterpret_cast<const unsigned char *>(iv.constData()), static_cast<size_t>(iv.size()), nullptr, 0,
        reinterpret_cast<const unsigned char *>(tag.constData()), tag.size(),
        reinterpret_cast<const unsigned char *>(cipherBody.constData()),
        reinterpret_cast<unsigned char *>(plainText.data()));
    mbedtls_gcm_free(&gcm);
    if (err != 0)
        return QByteArray();
    return plainText;
}
