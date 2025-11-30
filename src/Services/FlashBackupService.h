#ifndef FLASH_BACKUP_SERVICE_H
#define FLASH_BACKUP_SERVICE_H

#include <SPIFFS.h>
#include <string>
#include <vector>

class FlashBackupService {
public:
    FlashBackupService() = default;

    bool begin();
    void close();
    bool writeBackup(const std::string& vaultName, const std::vector<uint8_t>& data);
    std::vector<uint8_t> readBackup(const std::string& vaultName);
    bool backupExists(const std::string& vaultName);
    bool deleteBackup(const std::string& vaultName);
    std::vector<std::string> listBackups();

private:
    bool ensureDir();
    std::string backupPath(const std::string& vaultName) const;

    const char* baseDir = "/vault_backups";
};

#endif // FLASH_BACKUP_SERVICE_H
