#include "CryptoService.h"
#include <mbedtls/sha256.h>
#include <cstring>
#include <algorithm>
#include <esp_random.h>
#include "bootloader_random.h"
#include <utility>

CryptoService::CryptoService() {}

std::vector<uint8_t> CryptoService::generateHardwareRandom(size_t size) {
    // Get entropy from esp32 HRNG
    std::vector<uint8_t> randomData(size);
    bootloader_random_enable();
    esp_fill_random(randomData.data(), randomData.size());
    bootloader_random_disable();
    return randomData;
}

std::string CryptoService::generateRandomString(size_t length) {
    const std::string PRINTABLE_CHARACTERS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!@#$&*-_=+";

    auto randomData = generateHardwareRandom(length);
    std::string randomString;
    for (size_t i = 0; i < length; i++) {
        randomString += PRINTABLE_CHARACTERS[randomData[i] % PRINTABLE_CHARACTERS.size()];
    }
    return randomString;
}

std::vector<uint8_t> CryptoService::deriveKeyFromPassphrase(const std::string& passphrase, const std::vector<uint8_t>& salt, size_t keySize) {
    static constexpr int PBKDF2_ITERATIONS = 10000;
    std::vector<uint8_t> key(keySize);

    mbedtls_md_context_t mdContext;
    mbedtls_md_init(&mdContext);
    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    if (mbedtls_md_setup(&mdContext, mdInfo, 1) != 0) {
        mbedtls_md_free(&mdContext);
        throw std::runtime_error("Failed to setup MD context.");
    }

    int ret = mbedtls_pkcs5_pbkdf2_hmac(
        &mdContext,
        reinterpret_cast<const unsigned char*>(passphrase.data()), passphrase.size(),
        salt.data(), salt.size(),
        PBKDF2_ITERATIONS,
        keySize,
        key.data());

    mbedtls_md_free(&mdContext);

    if (ret != 0) {
        throw std::runtime_error("Failed to derive key using PBKDF2.");
    }

    return key;
}

std::vector<uint8_t> CryptoService::generateSalt(size_t saltSize) {
    std::vector<uint8_t> salt(saltSize);
    bootloader_random_enable();
    esp_fill_random(salt.data(), salt.size());
    bootloader_random_disable();
    return salt;
}

std::vector<uint8_t> CryptoService::generateNonce(size_t nonceSize) {
    return generateSalt(nonceSize);
}

VaultAeadBlob CryptoService::encryptVault(const std::string& data, const std::string& passphrase) {
    static constexpr size_t KEY_SIZE = 32; // AES-256
    auto& globalState = GlobalState::getInstance();

    std::vector<uint8_t> salt = generateSalt(globalState.getSaltSize());
    std::vector<uint8_t> key = deriveKeyFromPassphrase(passphrase, salt, KEY_SIZE);
    std::vector<uint8_t> nonce = generateNonce(globalState.getNonceSize());
    std::vector<uint8_t> tag(globalState.getTagSize());
    std::vector<uint8_t> ciphertext(data.size());

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key.data(), KEY_SIZE * 8);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        throw std::runtime_error("Failed to set AES-GCM key");
    }

    ret = mbedtls_gcm_crypt_and_tag(
        &gcm,
        MBEDTLS_GCM_ENCRYPT,
        data.size(),
        nonce.data(),
        nonce.size(),
        nullptr,
        0,
        reinterpret_cast<const unsigned char*>(data.data()),
        ciphertext.data(),
        tag.size(),
        tag.data());

    mbedtls_gcm_free(&gcm);

    if (ret != 0) {
        throw std::runtime_error("AES-GCM encryption failed");
    }

    return VaultAeadBlob{std::move(salt), std::move(nonce), std::move(tag), std::move(ciphertext)};
}

std::string CryptoService::decryptVault(const VaultAeadBlob& blob, const std::string& passphrase) {
    static constexpr size_t KEY_SIZE = 32; // AES-256
    std::vector<uint8_t> key = deriveKeyFromPassphrase(passphrase, blob.salt, KEY_SIZE);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key.data(), KEY_SIZE * 8);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return "";
    }

    std::vector<uint8_t> plaintext(blob.ciphertext.size());
    ret = mbedtls_gcm_auth_decrypt(
        &gcm,
        blob.ciphertext.size(),
        blob.nonce.data(),
        blob.nonce.size(),
        nullptr,
        0,
        blob.tag.data(),
        blob.tag.size(),
        blob.ciphertext.data(),
        plaintext.data());

    mbedtls_gcm_free(&gcm);

    if (ret != 0) {
        return "";
    }

    return std::string(plaintext.begin(), plaintext.end());
}
