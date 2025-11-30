#include "UtilityController.h"
#if defined(USE_NIMBLE)
#include <NimBLEDevice.h>
#endif

UtilityController::UtilityController(IView& display,
                                     IInput& input,
                                     HorizontalSelector& horizontalSelector,
                                     VerticalSelector& verticalSelector,
                                     FieldEditorSelector& fieldEditorSelector,
                                     StringPromptSelector& stringPromptSelector,
                                     ConfirmationSelector& confirmationSelector,
                                     UsbService& usbService,
                                     BleService& bleService,
                                     LedService& ledService,
                                     NvsService& nvsService,
                                     SdService& sdService,
                                     TimeTransformer& timeTransformer)
    : display(display),
      input(input),
      usbService(usbService),
      bleService(bleService),
      ledService(ledService),
      nvsService(nvsService),
      sdService(sdService),
      timeTransformer(timeTransformer),
      horizontalSelector(horizontalSelector),
      verticalSelector(verticalSelector),
      fieldEditorSelector(fieldEditorSelector),
      stringPromptSelector(stringPromptSelector),
      confirmationSelector(confirmationSelector) {}

void UtilityController::handleWelcome() {
    auto brightness = globalState.getSelectedScreenBrightness();
    display.welcome(brightness);
    input.waitPress();
    display.setBrightness(brightness);
}

bool UtilityController::handleSendKeystrokes(const std::string& sendString) {
    ledService.showLed();
    bool sent = false;

    if (globalState.getBleKeyboardEnabled()) {
        bleService.setLayout(KeyboardLayoutMapper::toLayout(globalState.getSelectedKeyboardLayout()));
        bleService.setDeviceName(globalState.getBleDeviceName());
        bleService.begin();
        display.subMessage("Sent keystrokes (BLE)", 0);
        sent = bleService.sendString(sendString);
    }

    if (!sent) {
        display.subMessage("Sent keystrokes (USB)", 0);
        usbService.sendString(sendString);
        sent = usbService.isReady();
    }

    ledService.clearLed();
    return sent;
}

bool UtilityController::handleKeyboardInitialization() {
    auto selectedKeyboardLayout = globalState.getSelectedKeyboardLayout();
    const uint8_t* finalLayout = KeyboardLayoutMapper::toLayout(selectedKeyboardLayout);
    auto nvsKeyboardLayoutField = globalState.getNvsKeyboardLayout();
    int selectedIndex;

    if (selectedKeyboardLayout.empty()) {
        selectedKeyboardLayout = nvsService.getString(nvsKeyboardLayoutField);
    }

    if (selectedKeyboardLayout.empty()) {
        auto layouts = KeyboardLayoutMapper::getAllLayoutNames();
        selectedIndex = horizontalSelector.select("Choose Keyboard", layouts, "Region Layout", "Press OK to select");
        if (selectedIndex == -1) {
            return false;
        }
        selectedKeyboardLayout = layouts[selectedIndex];
        nvsService.saveString(nvsKeyboardLayoutField, selectedKeyboardLayout);
        globalState.setSelectedKeyboardLayout(selectedKeyboardLayout);
        finalLayout = KeyboardLayoutMapper::toLayout(selectedKeyboardLayout);
    }
    usbService.setLayout(finalLayout);
    usbService.begin();
    if (globalState.getBleKeyboardEnabled()) {
        bleService.setLayout(finalLayout);
        bleService.setDeviceName(globalState.getBleDeviceName());
        bleService.begin();
    } else {
        bleService.end();
    }
    return true;
}

void UtilityController::handleLoadNvs() {
    // Keyboard layout
    std::string savedLayout = nvsService.getString(globalState.getNvsKeyboardLayout());
    if (!savedLayout.empty()) {
        globalState.setSelectedKeyboardLayout(savedLayout);
    }

    // Brightness
    std::string savedBrightness = nvsService.getString(globalState.getNvsScreenBrightness());
    if (!savedBrightness.empty()) {
        uint8_t brightness = std::stoi(savedBrightness);
        globalState.setSelectedScreenBrightness(brightness);
        display.setBrightness(brightness);
    }

    // Screen off
    std::string savedScreenTimeout = nvsService.getString(globalState.getNvsInactivityScreenTimeout());
    if (!savedScreenTimeout.empty()) {
        uint32_t screenTimeout = timeTransformer.toMilliseconds(savedScreenTimeout);
        if (screenTimeout > 0) {
            globalState.setInactivityScreenTimeout(screenTimeout);
        }
    }

    // Vault lock
    std::string savedLockTimeout = nvsService.getString(globalState.getNvsInactivityLockTimeout());
    if (!savedLockTimeout.empty()) {
        uint32_t lockTimeout = timeTransformer.toMilliseconds(savedLockTimeout);
        if (lockTimeout > 0) {
            globalState.setInactivityLockTimeout(lockTimeout);
        }
    }

    // BLE keyboard enable
    std::string savedBleEnabled = nvsService.getString(globalState.getNvsBleEnabled());
    if (!savedBleEnabled.empty()) {
        globalState.setBleKeyboardEnabled(savedBleEnabled == "1");
    }

    // BLE device name
    std::string savedBleDeviceName = nvsService.getString(globalState.getNvsBleDeviceName());
    if (!savedBleDeviceName.empty()) {
        globalState.setBleDeviceName(savedBleDeviceName);
    }
    bleService.setDeviceName(globalState.getBleDeviceName());
}

bool UtilityController::handleGeneralSettings() {
    std::vector<std::string> timeLabels = timeTransformer.getAllTimeLabels();
    std::vector<uint32_t> timeValues = timeTransformer.getAllTimeValues();
    std::vector<std::string> brightnessValues = {"20", "60", "100", "140", "160", "200", "240"};
    std::vector<std::string> settingLabels = {" Keyboard ", "Brightness", "Screen off", "Vault lock", " BLE ", "BLE name", "Forget BLE", "Clear BLE"};
    
    auto layouts = KeyboardLayoutMapper::getAllLayoutNames();
    auto selectedLayout = globalState.getSelectedKeyboardLayout().empty() ? layouts[2] : globalState.getSelectedKeyboardLayout();
    auto selectedScreenOffTime = timeTransformer.toLabel(globalState.getInactivityScreenTimeout());
    auto selectedLockCloseTime = timeTransformer.toLabel(globalState.getInactivityLockTimeout());
    std::vector<std::string> settings = {
        selectedLayout,
        std::to_string(globalState.getSelectedScreenBrightness()),
        selectedScreenOffTime, 
        selectedLockCloseTime + " ", // hack to prevent same values
        globalState.getBleKeyboardEnabled() ? "On" : "Off",
        globalState.getBleDeviceName(),
        "Forget",
        "Reset"
    };

    while (true) {
        auto verticalIndex = verticalSelector.select("Settings", settings, true, false, settingLabels, {});
        size_t selectedIndex;
        if (verticalIndex == -1) {
            return false;
        }

        auto selectedSetting =  settingLabels[verticalIndex];

        if (selectedSetting == " Keyboard ") {
            selectedIndex = horizontalSelector.select("Choose Keyboard", layouts, "Region Layout", "Press OK to select", {}, false);
            globalState.setSelectedKeyboardLayout(layouts[selectedIndex]);
            nvsService.saveString(globalState.getNvsKeyboardLayout(), layouts[selectedIndex]);
            settings[verticalIndex] = layouts[selectedIndex];

        } else if (selectedSetting == "Brightness") {
            selectedIndex = horizontalSelector.select("Screen Brightness", brightnessValues, "Choose brightness", "Press OK to select", {}, false);
            uint8_t brightness = std::stoi(brightnessValues[selectedIndex]);
            globalState.setSelectedScreenBrightness(brightness);
            nvsService.saveString(globalState.getNvsScreenBrightness(), brightnessValues[selectedIndex]);
            display.setBrightness(brightness);
            settings[verticalIndex] = brightnessValues[selectedIndex];

        } else if (selectedSetting == "Screen off")  {
            selectedIndex = horizontalSelector.select("Screen Off", timeLabels, "Turn off inactivity", "Press OK to select", {}, false);
            globalState.setInactivityScreenTimeout(timeValues[selectedIndex]);
            nvsService.saveString(globalState.getNvsInactivityScreenTimeout(), timeLabels[selectedIndex]);
            settings[verticalIndex] = timeLabels[selectedIndex] + "  "; // hack to avoid same values in the vector

        } else if (selectedSetting == "Vault lock") {
            selectedIndex = horizontalSelector.select("Vault Lock", timeLabels, "Lock vault inactivity", "Press OK to select", {}, false);
            globalState.setInactivityLockTimeout(timeValues[selectedIndex]);
            nvsService.saveString(globalState.getNvsInactivityLockTimeout(), timeLabels[selectedIndex]);
            settings[verticalIndex] = timeLabels[selectedIndex] + " ";
        } else if (selectedSetting == " BLE ") {
            std::vector<std::string> options = {"On", "Off"};
            selectedIndex = horizontalSelector.select("BLE Keyboard", options, "Enable BLE keyboard", "Press OK to select", {}, false);
            bool enableBle = options[selectedIndex] == "On";
            globalState.setBleKeyboardEnabled(enableBle);
            nvsService.saveString(globalState.getNvsBleEnabled(), enableBle ? "1" : "0");
            settings[verticalIndex] = options[selectedIndex];
            const uint8_t* layoutPtr = KeyboardLayoutMapper::toLayout(globalState.getSelectedKeyboardLayout());
            bleService.setLayout(layoutPtr);
            bleService.setDeviceName(globalState.getBleDeviceName());
            if (enableBle) {
                bleService.begin();
            } else {
                bleService.end();
            }
        } else if (selectedSetting == "BLE name") {
            auto newName = stringPromptSelector.select("BLE Name", "Device name", globalState.getBleDeviceName(), false, true, false, 0, false);
            if (!newName.empty() && newName != globalState.getBleDeviceName()) {
                globalState.setBleDeviceName(newName);
                nvsService.saveString(globalState.getNvsBleDeviceName(), newName);
                bleService.setDeviceName(newName);
                settings[verticalIndex] = newName;
            }
        } else if (selectedSetting == "Clear BLE") {
            auto confirm = confirmationSelector.select("Clear BLE Bonds", "Remove paired devices?");
            if (confirm) {
                bleService.clearBonds();
                settings[verticalIndex] = "Reset";
            }
        } else if (selectedSetting == "Forget BLE") {
            auto confirm = confirmationSelector.select("Forget BLE", "Disconnect and forget current peer?");
            if (confirm) {
#if defined(USE_NIMBLE)
                NimBLEDevice::stopAdvertising();
#endif
                bleService.clearBonds();
                bleService.end();
                bleService.begin(); // restart advertising for new connections
                settings[verticalIndex] = "Forget";
            }
        }
    }
}

void UtilityController::handleInactivity() {
    ledService.showLed();
    input.waitPress();
    ledService.clearLed();
    display.setBrightness(globalState.getSelectedScreenBrightness());
    if (!globalState.getLoadedVaultPath().empty()) {
        display.topBar("Inactivity", false, false);
        display.subMessage("Vault has been locked", 3000);
        globalState.setLoadedVaultPath("");
        globalState.setLoadedVaultPassword("");
        globalState.setVaultIsLocked(false);
    }
}
