#include "SdService.h"

SdService::SdService() {}

bool SdService::begin() {
    sdCardSPI.begin(
        globalState.getSdCardCLKPin(),
        globalState.getSdCardMISOPin(),
        globalState.getSdCardMOSIPin(),
        globalState.getSdCardCSPin()
    );
    delay(10);

    if (!SD.begin(globalState.getSdCardCSPin(), sdCardSPI)) {
        sdCardMounted = false;
        return false;
    }

    sdCardMounted = true;
    return sdCardMounted;
}

void SdService::close() {
    SD.end();
    sdCardMounted = false;
}

bool SdService::isFile(const std::string& filePath) {
    File f = SD.open(filePath.c_str());
    if (f && !f.isDirectory()) {
        f.close();
        return true;
    }
    return false;
}

bool SdService::isDirectory(const std::string& path) {
    File f = SD.open(path.c_str());
    if (f && f.isDirectory()) {
        f.close();
        return true;
    }
    return false;
}

bool SdService::getSdState() {
    return sdCardMounted;
}

std::vector<std::string> SdService::listElements(const std::string& dirPath, size_t limit) {
    if (limit == 0) {
        limit = globalState.getFileCountLimit();
    }

    std::vector<std::string> filesList;
    std::vector<std::string> foldersList;

    if (!sdCardMounted) {
        return filesList;
    }

    File dir = SD.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) {
        return filesList;
    }

    File file = dir.openNextFile();
    size_t i = 0;
    while (file) {
        if (file.name()[0] != '.') {  // Exclude hidden files
            if (file.isDirectory()) {
                foldersList.push_back(file.name());
            } else {
                filesList.push_back(file.name());
            }
            i++;
        }

        if (i > limit) {
            break;
        }

        file = dir.openNextFile();
    }

    std::sort(foldersList.begin(), foldersList.end());
    std::sort(filesList.begin(), filesList.end());
    foldersList.insert(foldersList.end(), filesList.begin(), filesList.end());

    return foldersList;
}

std::vector<uint8_t> SdService::readBinaryFile(const std::string& filePath) {
    std::vector<uint8_t> content;
    if (!sdCardMounted) {
        return content;
    }

    File file = SD.open(filePath.c_str(), FILE_READ);
    if (file) {
        content.reserve(file.size());
        while (file.available()) {
            content.push_back(file.read());
        }
        file.close();
    }
    return content;
}

std::string SdService::readFile(const std::string& filePath) {
    std::string content;
    if (!sdCardMounted) {
        return content;
    }

    File file = SD.open(filePath.c_str());
    if (file) {
        while (file.available()) {
            content += static_cast<char>(file.read());
        }
        file.close();
    }
    return content;
}

bool SdService::writeFile(const std::string& filePath, const std::string& data) {
    if (!sdCardMounted) {
        return false;
    }

    File file = SD.open(filePath.c_str(), FILE_WRITE);
    if (file) {
        file.write(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
        file.close();
        return true;
    }
    return false;
}

bool SdService::writeBinaryFile(const std::string& filePath, const std::vector<uint8_t>& data) {
    if (!sdCardMounted) {
        return false;
    }

    File file = SD.open(filePath.c_str(), FILE_WRITE);
    if (file) {
        file.write(data.data(), data.size());
        file.close();
        return true;
    }
    return false;
}

bool SdService::atomicWriteBinaryFileWithBackup(const std::string& filePath, const std::vector<uint8_t>& data) {
    if (!sdCardMounted) {
        return false;
    }

    std::string tmpPath = filePath + ".tmp";
    std::string bakPath = filePath + ".bak";

    // Write to temp
    File tmp = SD.open(tmpPath.c_str(), FILE_WRITE);
    if (!tmp) {
        return false;
    }
    tmp.write(data.data(), data.size());
    tmp.close();

    // Rotate backup: remove old .bak, rename current to .bak if exists
    if (SD.exists(bakPath.c_str())) {
        SD.remove(bakPath.c_str());
    }
    if (SD.exists(filePath.c_str())) {
        SD.rename(filePath.c_str(), bakPath.c_str());
    }

    // Rename temp to final
    bool renamed = SD.rename(tmpPath.c_str(), filePath.c_str());
    if (!renamed) {
        // Cleanup temp if failed
        SD.remove(tmpPath.c_str());
    }
    return renamed;
}

bool SdService::appendToFile(const std::string& filePath, const std::string& data) {
    if (!sdCardMounted) {
        return false;
    }

    File file = SD.open(filePath.c_str(), FILE_APPEND);
    if (file) {
        file.write(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
        file.close();
        return true;
    }
    return false;
}

bool SdService::deleteFile(const std::string& filePath) {
    if (!sdCardMounted) {
        return false;
    }

    if (SD.exists(filePath.c_str())) {
        return SD.remove(filePath.c_str());
    }
    return false;
}

bool SdService::validateVaultFile(const std::string& filePath) {
    // Vérifie si l'extension correspond à un fichier de coffre
    return getFileExt(filePath) == "vault";
}

std::string SdService::getFileExt(const std::string& path) {
    size_t pos = path.find_last_of('.');
    return (pos != std::string::npos && pos < path.length() - 1) ? path.substr(pos + 1) : "";
}

std::string SdService::getParentDirectory(const std::string& path) {
    size_t pos = path.find_last_of('/');
    return (pos != std::string::npos && pos > 0) ? path.substr(0, pos) : "/";
}

std::vector<std::string> SdService::getCachedDirectoryElements(const std::string& path) {
    if (cachedDirectoryElements.find(path) != cachedDirectoryElements.end()) {
        return cachedDirectoryElements[path];
    }

    std::vector<std::string> elements = listElements(path);
    if (elements.size() > 4) {
        if (cachedDirectoryElements.size() >= globalState.getFileCacheLimit()) {
            cachedDirectoryElements.erase(cachedDirectoryElements.begin());
        }
        cachedDirectoryElements[path] = elements;
    }
    return elements;
}

void SdService::setCachedDirectoryElements(const std::string& path, const std::vector<std::string>& elements) {
    cachedDirectoryElements[path] = elements;
}

void SdService::removeCachedPath(const std::string& path) {
    cachedDirectoryElements.erase(path);
}

std::string SdService::getFileName(const std::string& path) {
    size_t lastSlash = path.find_last_of('/');
    size_t lastDot = path.find_last_of('.');
    if (lastDot == std::string::npos || lastDot < lastSlash) {
        lastDot = path.length(); // Not ext, get end of path
    }
    size_t start = (lastSlash != std::string::npos) ? lastSlash + 1 : 0;
    return path.substr(start, lastDot - start);
}

bool SdService::ensureDirectory(const std::string& directory) {
    if (!sdCardMounted) {
        return false;
    }

    if (!SD.exists(directory.c_str())) {
        return SD.mkdir(directory.c_str()); // Create forlder
    }
    return true; // Folder already exists
}
