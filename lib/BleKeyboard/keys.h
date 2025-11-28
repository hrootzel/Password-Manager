#ifndef __KEYS_H__
#define __KEYS_H__

#include <Arduino.h>

#ifndef BAD_USB_KEYREPORT_DEFINED
#define BAD_USB_KEYREPORT_DEFINED
typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} KeyReport;
#endif

typedef uint8_t MediaKeyReport[2];

extern const uint8_t KeyboardLayout_de_DE[];
extern const uint8_t KeyboardLayout_en_US[];
extern const uint8_t KeyboardLayout_en_UK[];
extern const uint8_t KeyboardLayout_es_ES[];
extern const uint8_t KeyboardLayout_fr_FR[];
extern const uint8_t KeyboardLayout_it_IT[];
extern const uint8_t KeyboardLayout_pt_PT[];
extern const uint8_t KeyboardLayout_pt_BR[];
extern const uint8_t KeyboardLayout_sv_SE[];
extern const uint8_t KeyboardLayout_da_DK[];
extern const uint8_t KeyboardLayout_hu_HU[];

#ifndef KEY_LEFT_CTRL
#define KEY_LEFT_CTRL 0x80
#endif
#ifndef KEY_LEFT_SHIFT
#define KEY_LEFT_SHIFT 0x81
#endif
#ifndef KEY_LEFT_ALT
#define KEY_LEFT_ALT 0x82
#endif
#ifndef KEY_LEFT_GUI
#define KEY_LEFT_GUI 0x83
#endif
#ifndef KEY_RIGHT_CTRL
#define KEY_RIGHT_CTRL 0x84
#endif
#ifndef KEY_RIGHT_SHIFT
#define KEY_RIGHT_SHIFT 0x85
#endif
#ifndef KEY_RIGHT_ALT
#define KEY_RIGHT_ALT 0x86
#endif
#ifndef KEY_RIGHT_GUI
#define KEY_RIGHT_GUI 0x87
#endif

#ifndef KEY_UP_ARROW
#define KEY_UP_ARROW 0xDA
#endif
#ifndef KEY_DOWN_ARROW
#define KEY_DOWN_ARROW 0xD9
#endif
#ifndef KEY_LEFT_ARROW
#define KEY_LEFT_ARROW 0xD8
#endif
#ifndef KEY_RIGHT_ARROW
#define KEY_RIGHT_ARROW 0xD7
#endif
#ifndef KEYBACKSPACE
#define KEYBACKSPACE 0xB2 // changed from KEY_BACKSPACE due to compatibility to cardputer keyboard
#endif
#ifndef KEYTAB
#define KEYTAB 0xB3
#endif
#ifndef KEY_RETURN
#define KEY_RETURN 0xB0
#endif
#ifndef KEY_ESC
#define KEY_ESC 0xB1
#endif
#ifndef KEY_PRINT_SCREEN
#define KEY_PRINT_SCREEN 0xCE
#endif
#ifndef KEY_SCROLL_LOCK
#define KEY_SCROLL_LOCK 0xCF
#endif
#ifndef KEY_PAUSE
#define KEY_PAUSE 0xD0
#endif
#ifndef KEY_INSERT
#define KEY_INSERT 0xD1
#endif
#ifndef KEY_DELETE
#define KEY_DELETE 0xD4
#endif
#ifndef KEY_PAGE_UP
#define KEY_PAGE_UP 0xD3
#endif
#ifndef KEY_PAGE_DOWN
#define KEY_PAGE_DOWN 0xD6
#endif
#ifndef KEY_HOME
#define KEY_HOME 0xD2
#endif
#ifndef KEY_END
#define KEY_END 0xD5
#endif
#ifndef KEY_MENU
#define KEY_MENU 0xED
#endif
#ifndef KEY_CAPS_LOCK
#define KEY_CAPS_LOCK 0xC1
#endif
#ifndef KEY_F1
#define KEY_F1 0xC2
#endif
#ifndef KEY_F2
#define KEY_F2 0xC3
#endif
#ifndef KEY_F3
#define KEY_F3 0xC4
#endif
#ifndef KEY_F4
#define KEY_F4 0xC5
#endif
#ifndef KEY_F5
#define KEY_F5 0xC6
#endif
#ifndef KEY_F6
#define KEY_F6 0xC7
#endif
#ifndef KEY_F7
#define KEY_F7 0xC8
#endif
#ifndef KEY_F8
#define KEY_F8 0xC9
#endif
#ifndef KEY_F9
#define KEY_F9 0xCA
#endif
#ifndef KEY_F10
#define KEY_F10 0xCB
#endif
#ifndef KEY_F11
#define KEY_F11 0xCC
#endif
#ifndef KEY_F12
#define KEY_F12 0xCD
#endif
#ifndef KEY_F13
#define KEY_F13 0xF0
#endif
#ifndef KEY_F14
#define KEY_F14 0xF1
#endif
#ifndef KEY_F15
#define KEY_F15 0xF2
#endif
#ifndef KEY_F16
#define KEY_F16 0xF3
#endif
#ifndef KEY_F17
#define KEY_F17 0xF4
#endif
#ifndef KEY_F18
#define KEY_F18 0xF5
#endif
#ifndef KEY_F19
#define KEY_F19 0xF6
#endif
#ifndef KEY_F20
#define KEY_F20 0xF7
#endif
#ifndef KEY_F21
#define KEY_F21 0xF8
#endif
#ifndef KEY_F22
#define KEY_F22 0xF9
#endif
#ifndef KEY_F23
#define KEY_F23 0xFA
#endif
#ifndef KEY_F24
#define KEY_F24 0xFB
#endif

#ifndef LED_NUMLOCK
#define LED_NUMLOCK 0x01
#endif
#ifndef LED_CAPSLOCK
#define LED_CAPSLOCK 0x02
#endif
#ifndef LED_SCROLLLOCK
#define LED_SCROLLLOCK 0x04
#endif
#ifndef LED_COMPOSE
#define LED_COMPOSE 0x08
#endif
#ifndef LED_KANA
#define LED_KANA 0x10
#endif
#ifndef KEY_SPACE
#define KEY_SPACE 0x2c
#endif

const MediaKeyReport KEY_MEDIA_NEXT_TRACK = {1, 0};
const MediaKeyReport KEY_MEDIA_PREVIOUS_TRACK = {2, 0};
const MediaKeyReport KEY_MEDIA_STOP = {4, 0};
const MediaKeyReport KEY_MEDIA_PLAY_PAUSE = {8, 0};
const MediaKeyReport KEY_MEDIA_MUTE = {16, 0};
const MediaKeyReport KEY_MEDIA_VOLUME_UP = {32, 0};
const MediaKeyReport KEY_MEDIA_VOLUME_DOWN = {64, 0};
const MediaKeyReport KEY_MEDIA_WWW_HOME = {128, 0};
const MediaKeyReport KEY_MEDIA_LOCAL_MACHINE_BROWSER = {0, 1}; // Opens "My Computer" on Windows
const MediaKeyReport KEY_MEDIA_CALCULATOR = {0, 2};
const MediaKeyReport KEY_MEDIA_WWW_BOOKMARKS = {0, 4};
const MediaKeyReport KEY_MEDIA_WWW_SEARCH = {0, 8};
const MediaKeyReport KEY_MEDIA_WWW_STOP = {0, 16};
const MediaKeyReport KEY_MEDIA_WWW_BACK = {0, 32};
const MediaKeyReport KEY_MEDIA_CONSUMER_CONTROL_CONFIGURATION = {0, 64}; // Media Selection
const MediaKeyReport KEY_MEDIA_EMAIL_READER = {0, 128};

#endif
