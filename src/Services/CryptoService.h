#ifndef CRYPTO_SERVICE_H
#define CRYPTO_SERVICE_H

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>
#include <States/GlobalState.h>

struct VaultAeadBlob {
    std::vector<uint8_t> salt;
    std::vector<uint8_t> nonce;
    std::vector<uint8_t> tag;
    std::vector<uint8_t> ciphertext;
};

class CryptoService {
public:
    CryptoService();

    // Key derivation and passphrase handling
    std::vector<uint8_t> deriveKeyFromPassphrase(const std::string& passphrase, const std::vector<uint8_t>& salt, size_t keySize);

    // Passphrase-based encryption/decryption of private data (AES-256-GCM)
    VaultAeadBlob encryptVault(const std::string& data, const std::string& passphrase);
    std::string decryptVault(const VaultAeadBlob& blob, const std::string& passphrase);

    // Utility
    std::vector<uint8_t> generateSalt(size_t saltSize);
    std::vector<uint8_t> generateNonce(size_t nonceSize);
    std::vector<uint8_t> generateHardwareRandom(size_t size);
    std::string generateRandomString(size_t length);
};

#endif // CRYPTO_SERVICE_H
