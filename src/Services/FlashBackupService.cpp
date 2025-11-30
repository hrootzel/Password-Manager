#include "FlashBackupService.h"

bool FlashBackupService::begin() {
    return SPIFFS.begin(true);
}

void FlashBackupService::close() {
    SPIFFS.end();
}

bool FlashBackupService::ensureDir() {
    if (SPIFFS.exists(baseDir)) {
        return true;
    }
    return SPIFFS.mkdir(baseDir);
}

std::string FlashBackupService::backupPath(const std::string& vaultName) const {
    return std::string(baseDir) + "/" + vaultName + ".vault";
}

bool FlashBackupService::writeBackup(const std::string& vaultName, const std::vector<uint8_t>& data) {
    if (!ensureDir()) {
        return false;
    }

    // Space check: avoid filling SPIFFS completely
    size_t freeBytes = SPIFFS.totalBytes() > SPIFFS.usedBytes() ? (SPIFFS.totalBytes() - SPIFFS.usedBytes()) : 0;
    if (data.size() > freeBytes) {
        return false;
    }

    const std::string destPath = backupPath(vaultName);
    const std::string tmpPath = destPath + ".tmp";
    const std::string bakPath = destPath + ".bak";

    File tmp = SPIFFS.open(tmpPath.c_str(), FILE_WRITE);
    if (!tmp) {
        return false;
    }
    tmp.write(data.data(), data.size());
    tmp.close();

    if (SPIFFS.exists(bakPath.c_str())) {
        SPIFFS.remove(bakPath.c_str());
    }
    if (SPIFFS.exists(destPath.c_str())) {
        SPIFFS.rename(destPath.c_str(), bakPath.c_str());
    }

    bool renamed = SPIFFS.rename(tmpPath.c_str(), destPath.c_str());
    if (!renamed) {
        SPIFFS.remove(tmpPath.c_str());
    }
    return renamed;
}

std::vector<uint8_t> FlashBackupService::readBackup(const std::string& vaultName) {
    std::vector<uint8_t> data;
    const std::string path = backupPath(vaultName);
    if (!SPIFFS.exists(path.c_str())) {
        return data;
    }
    File file = SPIFFS.open(path.c_str(), FILE_READ);
    if (!file) {
        return data;
    }
    data.reserve(file.size());
    while (file.available()) {
        data.push_back(file.read());
    }
    file.close();
    return data;
}

bool FlashBackupService::backupExists(const std::string& vaultName) {
    return SPIFFS.exists(backupPath(vaultName).c_str());
}

bool FlashBackupService::deleteBackup(const std::string& vaultName) {
    return SPIFFS.remove(backupPath(vaultName).c_str());
}

std::vector<std::string> FlashBackupService::listBackups() {
    std::vector<std::string> names;
    File dir = SPIFFS.open(baseDir);
    if (!dir || !dir.isDirectory()) {
        return names;
    }
    File file = dir.openNextFile();
    while (file) {
        std::string name = file.name();
        if (name.find(".vault") != std::string::npos) {
            size_t lastSlash = name.find_last_of('/');
            size_t start = (lastSlash != std::string::npos) ? lastSlash + 1 : 0;
            size_t dot = name.find_last_of('.');
            if (dot == std::string::npos || dot < start) {
                dot = name.size();
            }
            names.push_back(name.substr(start, dot - start));
        }
        file = dir.openNextFile();
    }
    return names;
}
