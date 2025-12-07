#pragma once

class KeyMapping {
public:
    enum class ScanCode {
#ifdef _WIN32
        DIK_1           = 0x02,
        DIK_2           = 0x03,
        DIK_3           = 0x04,
        DIK_4           = 0x05,
        DIK_5           = 0x06,
        DIK_6           = 0x07,
        DIK_7           = 0x08,
        DIK_8           = 0x09,
        DIK_9           = 0x0A,
        DIK_0           = 0x0B,
        DIK_MINUS       = 0x0C,
        DIK_EQUALS      = 0x0D,
        DIK_LBRACKET    = 0x1A,
        DIK_RBRACKET    = 0x1B,
        DIK_SEMICOLON   = 0x27,
        DIK_APOSTROPHE  = 0x28,
        DIK_GRAVE       = 0x29,
        DIK_BACKSLASH   = 0x2B,
        DIK_COMMA       = 0x33,
        DIK_PERIOD      = 0x34,
        DIK_SLASH       = 0x35
#else
        DIK_1           = 0x12, // kVK_ANSI_1
        DIK_2           = 0x13, // kVK_ANSI_2
        DIK_3           = 0x14, // kVK_ANSI_3
        DIK_4           = 0x15, // kVK_ANSI_4
        DIK_5           = 0x17, // kVK_ANSI_5
        DIK_6           = 0x16, // kVK_ANSI_6
        DIK_7           = 0x1A, // kVK_ANSI_7
        DIK_8           = 0x1C, // kVK_ANSI_8
        DIK_9           = 0x19, // kVK_ANSI_9
        DIK_0           = 0x1D, // kVK_ANSI_0
        
        DIK_MINUS       = 0x1B, // kVK_ANSI_Minus (-)
        DIK_EQUALS      = 0x18, // kVK_ANSI_Equal (=)
        DIK_LBRACKET    = 0x21, // kVK_ANSI_LeftBracket ([)
        DIK_RBRACKET    = 0x1E, // kVK_ANSI_RightBracket (])
        DIK_SEMICOLON   = 0x29, // kVK_ANSI_Semicolon (;)
        DIK_APOSTROPHE  = 0x27, // kVK_ANSI_Quote (')
        DIK_GRAVE       = 0x32, // kVK_ANSI_Grave (`)
        DIK_BACKSLASH   = 0x2A, // kVK_ANSI_Backslash (\)
        DIK_COMMA       = 0x2B, // kVK_ANSI_Comma (,)
        DIK_PERIOD      = 0x2F, // kVK_ANSI_Period (.)
        DIK_SLASH       = 0x2C  // kVK_ANSI_Slash (/)
#endif
    };

    enum class KeyChar {
        DIK_1           = '1',
        DIK_2           = '2',
        DIK_3           = '3',
        DIK_4           = '4',
        DIK_5           = '5',
        DIK_6           = '6',
        DIK_7           = '7',
        DIK_8           = '8',
        DIK_9           = '9',
        DIK_0           = '0',
        DIK_MINUS       = '-',
        DIK_EQUALS      = '=',
        DIK_LBRACKET    = '[',
        DIK_RBRACKET    = ']',
        DIK_SEMICOLON   = ';',
        DIK_APOSTROPHE  = '\'',
        DIK_GRAVE       = '`',
        DIK_BACKSLASH   = '\\',
        DIK_COMMA       = ',',
        DIK_PERIOD      = '.',
        DIK_SLASH       = '/'
    };

    static char toChar(int key) {
        switch (static_cast<ScanCode>(key)) {
            // --- 数字键 ---
            case ScanCode::DIK_1:           return asChar(KeyChar::DIK_1);
            case ScanCode::DIK_2:           return asChar(KeyChar::DIK_2);
            case ScanCode::DIK_3:           return asChar(KeyChar::DIK_3);
            case ScanCode::DIK_4:           return asChar(KeyChar::DIK_4);
            case ScanCode::DIK_5:           return asChar(KeyChar::DIK_5);
            case ScanCode::DIK_6:           return asChar(KeyChar::DIK_6);
            case ScanCode::DIK_7:           return asChar(KeyChar::DIK_7);
            case ScanCode::DIK_8:           return asChar(KeyChar::DIK_8);
            case ScanCode::DIK_9:           return asChar(KeyChar::DIK_9);
            case ScanCode::DIK_0:           return asChar(KeyChar::DIK_0);
            
            // --- 符号键 ---
            case ScanCode::DIK_MINUS:       return asChar(KeyChar::DIK_MINUS);
            case ScanCode::DIK_EQUALS:      return asChar(KeyChar::DIK_EQUALS);
            case ScanCode::DIK_LBRACKET:    return asChar(KeyChar::DIK_LBRACKET);
            case ScanCode::DIK_RBRACKET:    return asChar(KeyChar::DIK_RBRACKET);
            case ScanCode::DIK_SEMICOLON:   return asChar(KeyChar::DIK_SEMICOLON);
            case ScanCode::DIK_APOSTROPHE:  return asChar(KeyChar::DIK_APOSTROPHE);
            case ScanCode::DIK_GRAVE:       return asChar(KeyChar::DIK_GRAVE);
            case ScanCode::DIK_BACKSLASH:   return asChar(KeyChar::DIK_BACKSLASH);
            case ScanCode::DIK_COMMA:       return asChar(KeyChar::DIK_COMMA);
            case ScanCode::DIK_PERIOD:      return asChar(KeyChar::DIK_PERIOD);
            case ScanCode::DIK_SLASH:       return asChar(KeyChar::DIK_SLASH);
            
            default:                        return 0;
        }
    }

private:
    // 辅助函数：将强类型枚举转换为 char
    // constexpr 保证在编译期完成转换，无运行时开销
    static constexpr char asChar(KeyChar k) {
        return static_cast<char>(k);
    }
};