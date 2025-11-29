#include "VaultController.h"
#include <algorithm>

namespace {
void zeroString(std::string& s) {
    if (!s.empty()) {
        std::fill(s.begin(), s.end(), '\0');
        s.clear();
    }
}
} // namespace

VaultController::VaultController(IView& display, 
                                 IInput& input, 
                                 HorizontalSelector& horizontalSelector, 
                                 VerticalSelector& verticalSelector,
                                 ConfirmationSelector& confirmationSelector,
                                 StringPromptSelector& stringPromptSelector,
                                 SdService& sdService, 
                                 NvsService& nvsService, 
                                 CategoryService& categoryService, 
                                 EntryService& entryService,
                                 CryptoService& cryptoService,
                                 JsonTransformer& jsonTransformer,
                                 ModelTransformer& modelTransformer)
    : display(display), 
      input(input), 
      horizontalSelector(horizontalSelector), 
      verticalSelector(verticalSelector),
      confirmationSelector(confirmationSelector),
      stringPromptSelector(stringPromptSelector),
      sdService(sdService), 
      nvsService(nvsService), 
      categoryService(categoryService), 
      entryService(entryService),
      cryptoService(cryptoService),
      jsonTransformer(jsonTransformer),
      modelTransformer(modelTransformer) {}

VaultController::~VaultController() {
    globalState.clearSensitive();
}

ActionEnum VaultController::actionNoVault() {
    std::vector<ActionEnum> availableActions = {ActionEnum::OpenVault, ActionEnum::CreateVault, ActionEnum::UpdateSettings};
    auto actionIcons = {IconEnum::LoadVault, IconEnum::CreateVault, IconEnum::Settings};
    auto labels = ActionEnumMapper::getActionNames(availableActions);
    auto  iconNames = IconEnumMapper::getIconNames(actionIcons);
    auto selectedIndex = horizontalSelector.select("", labels, "", "", iconNames);

    if (selectedIndex != -1) {
        return availableActions[selectedIndex];
    }

    return ActionEnum::None;
}

ActionEnum VaultController::actionVaultSelected() {
    std::vector<ActionEnum> availableActions = {ActionEnum::SelectEntry, ActionEnum::CreateEntry, ActionEnum::DeleteEntry};
    auto actionIcons = {IconEnum::SelectEntry, IconEnum::AddEntry, IconEnum::DeleteEntry};
    auto labels = ActionEnumMapper::getActionNames(availableActions);
    auto iconNames = IconEnumMapper::getIconNames(actionIcons);

    auto confirmation = false;
    while (!confirmation) {
        auto selectedIndex = horizontalSelector.select("", labels, "", "", iconNames);
        if (selectedIndex != -1) {
            return availableActions[selectedIndex];
        }

        if (globalState.getVaultIsLocked()) {
            confirmation = true;
        } else  {
            confirmation = confirmationSelector.select("Back to the Menu", "Close the vault ?");
        }
    }

    return ActionEnum::None;
}


bool VaultController::handleVaultCreation() {
    // Check SD card
    display.topBar("Create a new vault", false, false);
    display.subMessage("Loading...", 500);
    if (!sdService.begin()) {
        display.subMessage("SD card not found", 2000);
        return false;
    }

    // Get a vault name
    auto vaultName = stringPromptSelector.select("Create a new vault", "Enter the vault name", "", true, false, true);
    if (vaultName.empty()) {
        return false; // back button hits
    }

    // Verify if a vault file with this name exists
    auto vaultPath = globalState.getDefaultVaultPath() + "/" + vaultName + ".vault";
    if (sdService.isFile(vaultPath)) {
        auto confirmation = confirmationSelector.select("Vault already exists", "Erase the vault ?");
        if (!confirmation) {
            return false;
        }
    }

    // Get the password
    std::string pass1 = "";
    std::string pass2 = "";
    do {
        pass1 = stringPromptSelector.select("Vault Password", "Enter master password", "", false, true);
        pass2 = stringPromptSelector.select("Repeat Password", "Repeat master password", "", false, true);
        if (pass1 != pass2) {display.subMessage("Do not match", 2000);}
    } while (pass1 != pass2);
    

    // Encrypt empty json struct
    display.subMessage("Creating vault...", 0);
    auto jsonEmpty = jsonTransformer.emptyJsonStructure();
    auto payload = cryptoService.encryptVault(jsonEmpty, pass1);

    // Create VaultFile to handle data
    VaultFile vault = VaultFile(vaultPath, {});
    vault.setSalt(payload.salt);
    vault.setNonce(payload.nonce);
    vault.setTag(payload.tag);
    vault.setEncryptedData(payload.ciphertext);
    entryService.setContainerName(vaultName);

    // Save to SD card
    sdService.begin();
    sdService.ensureDirectory(globalState.getDefaultVaultPath());
    auto confirmation = sdService.writeBinaryFile(vaultPath, vault.getData());
    sdService.close();
    if (!confirmation) {
        return false;
    }
    // New content in this directory, remove cached elements
    sdService.removeCachedPath(globalState.getDefaultVaultPath());

    // Update state
    globalState.setLoadedVaultPassword(pass1);
    globalState.setLoadedVaultPath(vaultPath);

    // Zero sensitive temporaries
    zeroString(pass2);
    zeroString(pass1);
    zeroString(jsonEmpty);
    std::fill(payload.salt.begin(), payload.salt.end(), 0);
    std::fill(payload.nonce.begin(), payload.nonce.end(), 0);
    std::fill(payload.tag.begin(), payload.tag.end(), 0);
    std::fill(payload.ciphertext.begin(), payload.ciphertext.end(), 0);
    return true;
}

bool VaultController::handleVaultSave() {
    // Verify if a vault is loaded
    auto loadedVaultPath = globalState.getLoadedVaultPath();
    auto loadedVaultPassword = globalState.getLoadedVaultPassword();
    if (loadedVaultPath.empty() || loadedVaultPassword.empty()) {
        display.subMessage("No vault loaded", 2000);
        return false;
    }

    // Load vault
    sdService.begin();
    auto vaultData = sdService.readBinaryFile(loadedVaultPath);
    sdService.close();
    if (vaultData.empty()) {
        display.subMessage("Failed to load vault", 2000);
        return false;
    }

    // Create VaultFile to handle data
    VaultFile vault(loadedVaultPath, vaultData);
    if (!vault.hasValidMagic()) {
        display.subMessage("Invalid vault data", 2000);
        return false;
    }
    std::fill(vaultData.begin(), vaultData.end(), 0);
    vaultData.clear();

    // Get up to date data
    auto entries = entryService.getAllEntries();
    auto categories = categoryService.getAllCategories();

    // tranform to JSON
    auto jsonData = jsonTransformer.mergeEntriesAndCategoriesToJson(entries, categories);

    // Encrypt data
    auto payload = cryptoService.encryptVault(jsonData, loadedVaultPassword);

    // Update VaultFile
    vault.setSalt(payload.salt);
    vault.setNonce(payload.nonce);
    vault.setTag(payload.tag);
    vault.setEncryptedData(payload.ciphertext);
    zeroString(jsonData);
    std::fill(payload.salt.begin(), payload.salt.end(), 0);
    std::fill(payload.nonce.begin(), payload.nonce.end(), 0);
    std::fill(payload.tag.begin(), payload.tag.end(), 0);
    std::fill(payload.ciphertext.begin(), payload.ciphertext.end(), 0);

    // Save
    sdService.begin();
    auto confirmation = sdService.writeBinaryFile(loadedVaultPath, vault.getData());
    sdService.close();

    if (!confirmation) {
        display.subMessage("Failed to save vault", 2000);
        return false;
    }

    return true;
}

bool VaultController::handleVaultLoading() {
    // Loading method
    std::vector<ActionEnum> availableActions = {ActionEnum::LoadSdVault};
    std::vector<IconEnum> actionIcons = {IconEnum::SdCard};
    auto actionLabels = ActionEnumMapper::getActionNames(availableActions);
    auto iconNames = IconEnumMapper::getIconNames(actionIcons);

    auto selectedIndex = horizontalSelector.select("", actionLabels, "", "", iconNames);
    if (selectedIndex == -1) {
        return false;
    }

    auto action = availableActions[selectedIndex];
    switch (action) {
        case ActionEnum::LoadSdVault:
            return loadSdVault();
    }
}

bool VaultController::loadSdVault() {
    display.topBar("Load the SD card", false, false);
    display.subMessage("Loading...", 500);
    if (!sdService.begin()) {
        display.subMessage("SD card not found", 2000);
        return false;
    }

    // Check path
    std::string currentPath = nvsService.getString(globalState.getNvsLastUsedVaultPath());
    currentPath = sdService.isDirectory(currentPath) ? currentPath : "";
    if (currentPath.empty()) {
        auto defaultPath = globalState.getDefaultVaultPath();
        currentPath = sdService.ensureDirectory(defaultPath) ? defaultPath : "/";
    }
    
    // Explore folder to find a .vault file
    std::vector<std::string> elementNames;
    do {
        // Current path is a file
        if (sdService.isFile(currentPath)) {
            if (sdService.validateVaultFile(currentPath)) {
                if (loadDataFromEncryptedFile(currentPath)) {
                    display.subMessage("Loaded successfully", 2000);
                    return true;
                } else {
                    display.subMessage("Invalid Password", 2000);    
                }
            } else {
                display.subMessage("Invalid File", 2000);
            }
            
            // Not a valid file, revert to directory
            currentPath = sdService.getParentDirectory(currentPath);
            currentPath = currentPath.empty() ? "/" : currentPath;
        }
        
        // Load Elements
        display.subMessage("Loading...", 0);
        elementNames = sdService.getCachedDirectoryElements(currentPath);
        if (elementNames.empty()) {
            display.subMessage("No elements found", 2000);
            currentPath = sdService.getParentDirectory(currentPath);
            continue;
        }

        // Select Element
        uint16_t selectedIndex = verticalSelector.select(currentPath, elementNames, true, true, {} ,{}, false, false);
        if (selectedIndex >= elementNames.size()) {
            if (currentPath == "/") {
                sdService.close(); // return was made at root level
                return false;
            }
            currentPath = sdService.getParentDirectory(currentPath);

        } else if (!currentPath.empty() && currentPath.back() != '/') {
            currentPath += "/";
        }

        currentPath += elementNames[selectedIndex];
    } while (true);
}

bool VaultController::loadDataFromEncryptedFile(std::string path) {
    auto vaultBinary = sdService.readBinaryFile(path);
    VaultFile vaultFile = VaultFile(path, vaultBinary);
    if (!vaultFile.hasValidMagic()) {
        display.subMessage("Invalid vault data", 2000);
        return false;
    }
    std::fill(vaultBinary.begin(), vaultBinary.end(), 0);
    vaultBinary.clear();
    auto password = stringPromptSelector.select("Open encrypted vault", "Enter master password", "", false, true, false);
    display.subMessage("Loading...", 0);
    auto salt = vaultFile.getSalt();
    auto nonce = vaultFile.getNonce();
    auto tag = vaultFile.getTag();
    auto encryptedData = vaultFile.getEncryptedData();

    // Bad password
    VaultAeadBlob blob{salt, nonce, tag, encryptedData};
    auto decryptedData = cryptoService.decryptVault(blob, password);
    if (decryptedData.empty()) {
        return false;
    }

    auto vaultName = sdService.getFileName(path);
    auto entries = jsonTransformer.fromJsonToEntries(decryptedData);
    auto categories = jsonTransformer.fromJsonToCategories(decryptedData);
    entryService.setEntries(entries);
    entryService.setContainerName(vaultName);
    categoryService.setCategories(categories);
    globalState.setLoadedVaultPassword(password);
    globalState.setLoadedVaultPath(path);
    auto parentDir = sdService.getParentDirectory(path);
    nvsService.saveString(globalState.getNvsLastUsedVaultPath(), parentDir);

    // Zero sensitive temporaries
    zeroString(password);
    zeroString(decryptedData);
    std::fill(salt.begin(), salt.end(), 0);
    std::fill(nonce.begin(), nonce.end(), 0);
    std::fill(tag.begin(), tag.end(), 0);
    std::fill(encryptedData.begin(), encryptedData.end(), 0);
    return true;
}
