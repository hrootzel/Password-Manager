#ifndef VAULT_FILE_H
#define VAULT_FILE_H

#include <string>
#include <vector>
#include <algorithm>
#include <States/GlobalState.h>

class VaultFile {
private:
    std::string path; 
    std::vector<uint8_t> data; // raw data (magic + salt + nonce + tag + ciphertext)

    GlobalState& globalState = GlobalState::getInstance();

    void ensureHeader() {
        auto magic = globalState.getVaultMagic();
        const size_t magicSize = magic.size();
        if (data.size() < magicSize) {
            data.resize(magicSize, 0);
        }
        std::copy(magic.begin(), magic.end(), data.begin());
    }

    size_t magicSize() const { return globalState.getVaultMagic().size(); }
    size_t saltOffset() const { return magicSize(); }
    size_t nonceOffset() const { return saltOffset() + globalState.getSaltSize(); }
    size_t tagOffset() const { return nonceOffset() + globalState.getNonceSize(); }
    size_t cipherOffset() const { return tagOffset() + globalState.getTagSize(); }

public:
    // Constructeur
    VaultFile(const std::string& filePath, const std::vector<uint8_t>& rawData)
        : path(filePath), data(rawData) {}

    // Accesseurs
    const std::string& getPath() const { return path; }

    bool hasValidMagic() const {
        auto magic = globalState.getVaultMagic();
        if (data.size() < magic.size()) {
            return false;
        }
        return std::equal(magic.begin(), magic.end(), data.begin());
    }

    std::vector<uint8_t> getSalt() const {
        size_t saltSize = globalState.getSaltSize();
        size_t off = saltOffset();
        if (data.size() >= off + saltSize) {
            return {data.begin() + off, data.begin() + off + saltSize};
        }
        return {};
    }

    std::vector<uint8_t> getNonce() const {
        size_t nonceSize = globalState.getNonceSize();
        size_t off = nonceOffset();
        if (data.size() >= off + nonceSize) {
            return {data.begin() + off, data.begin() + off + nonceSize};
        }
        return {};
    }

    std::vector<uint8_t> getTag() const {
        size_t tagSize = globalState.getTagSize();
        size_t off = tagOffset();
        if (data.size() >= off + tagSize) {
            return {data.begin() + off, data.begin() + off + tagSize};
        }
        return {};
    }

    std::vector<uint8_t> getData() const {
        return data;
    }

    std::vector<uint8_t> getEncryptedData() const {
        size_t off = cipherOffset();
        if (data.size() > off) {
            return {data.begin() + off, data.end()};
        }
        return {};
    }

    // Mutateurs
    void setPath(const std::string& filePath) { path = filePath; }

    void setData(const std::vector<uint8_t>& rawData) { data = rawData; }

    void setSalt(const std::vector<uint8_t>& salt) {
        ensureHeader();
        size_t off = saltOffset();
        if (data.size() < off + salt.size()) {
            data.resize(off + salt.size());
        }
        std::copy(salt.begin(), salt.end(), data.begin() + off);
    }

    void setNonce(const std::vector<uint8_t>& nonce) {
        ensureHeader();
        size_t off = nonceOffset();
        if (data.size() < off + nonce.size()) {
            data.resize(off + nonce.size());
        }
        std::copy(nonce.begin(), nonce.end(), data.begin() + off);
    }

    void setTag(const std::vector<uint8_t>& tag) {
        ensureHeader();
        size_t off = tagOffset();
        if (data.size() < off + tag.size()) {
            data.resize(off + tag.size());
        }
        std::copy(tag.begin(), tag.end(), data.begin() + off);
    }

    void setEncryptedData(const std::vector<uint8_t>& encryptedData) {
        ensureHeader();
        size_t off = cipherOffset();
        if (data.size() < off) {
            data.resize(off);
        }
        data.erase(data.begin() + off, data.end());
        data.insert(data.end(), encryptedData.begin(), encryptedData.end());
    }

};

#endif // VAULT_FILE_H
