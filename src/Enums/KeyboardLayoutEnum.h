#ifndef KEYBOARD_LAYOUT_ENUM_H
#define KEYBOARD_LAYOUT_ENUM_H

#include <unordered_map>
#include <string>
#include <map>
#include <vector>
#include <KeyboardLayout.h>

enum class KeyboardLayoutEnum {
    EnglishUS,
    EnglishUK,
    FrenchFR,
    GermanDE,
    SpanishES,
    ItalianIT,
    PortuguesePT,
    PortugueseBR,
    SwedishSE,
    DanishDK,
    HungarianHU,
    None 
};

class KeyboardLayoutMapper {
public:
    // Associated label and layout
    inline static const std::map<std::string, const uint8_t*> layoutMap = {
        {"English (US)", KeyboardLayout_en_US},
        {"English (UK)", KeyboardLayout_en_UK},
        {"French (FR)", KeyboardLayout_fr_FR},
        {"German (DE)", KeyboardLayout_de_DE},
        {"Italian (IT)", KeyboardLayout_it_IT},
        {"Spanish (ES)", KeyboardLayout_es_ES},
        {"Portuguese (PT)", KeyboardLayout_pt_PT},
        {"Portuguese (BR)", KeyboardLayout_pt_BR},
        {"Danish (DK)", KeyboardLayout_da_DK},
        {"Swedish (SE)", KeyboardLayout_sv_SE},
        {"Hungarian (HU)", KeyboardLayout_hu_HU}
    };

    static const uint8_t* toLayout(const std::string& layoutName) {
        auto it = layoutMap.find(layoutName);
        return it != layoutMap.end() ? it->second : KeyboardLayout_en_US;
    }

    static std::vector<std::string> getAllLayoutNames() {
        std::vector<std::string> names;
        for (const auto& pair : layoutMap) {
            names.push_back(pair.first);
        }
        return names;
    }
};

#endif // KEYBOARD_LAYOUT_ENUM_H
