#include "FieldActionSelector.h"

FieldActionSelector::FieldActionSelector(IView& display, IInput& input, InactivityManager& inactivityManager)
    : display(display), input(input), inactivityManager(inactivityManager) {}

ActionEnum FieldActionSelector::select(const std::string& label, const std::string& value) {
    char key = KEY_NONE;
    inactivityManager.reset();

    display.topBar(label, true, false);
    bool isPasswordField = (label == "Pass");
    bool reveal = false;
    auto masked = isPasswordField ? std::string(value.size(), '*') : value;
    display.value(label, masked);

    while (true) {        
        inactivityManager.update();

        if (inactivityManager.getVaultIsLocked()) {
            return ActionEnum::None;
        }

        key = input.handler();

        if (key != KEY_NONE) {
            inactivityManager.reset();
        }

        switch (key) {
            case KEY_ESC_CUSTOM:
            case KEY_ARROW_LEFT:
                return ActionEnum::None;
            case KEY_OK:
                return ActionEnum::SendToUsb;
            case 'm':
                return ActionEnum::UpdateField;
            case 'v':
                if (isPasswordField) {
                    reveal = !reveal;
                    display.value(label, reveal ? value : masked);
                }
                break;
        }
    }
}
