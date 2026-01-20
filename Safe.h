#pragma once

#include <cstdint>

namespace StringGuard {

    // 编译期哈希：生成基于时间+行号的伪随机 Key
    constexpr uint32_t compileTimeHash(const char* str, uint32_t seed) {
        return *str ? compileTimeHash(str + 1, (seed ^ *str) * 16777619u) : seed;
    }

    constexpr uint32_t getSeed(int line) {
        return compileTimeHash(__TIME__, line);
    }

    template <uint32_t N = 64>
    struct Obfuscator {
        // 使用 mutable 允许在 const 对象中解密 (用于隐式转换)
        mutable char m_buffer[N];
        uint32_t m_key;

        consteval Obfuscator(const char(&str)[N]) : m_buffer{}, m_key(getSeed(__LINE__))
        {
            encrypt(str);
        }

        // 统一的加密逻辑
        constexpr void encrypt(const char(&str)[N]) {
            for (uint32_t i = 0; i < N; ++i) {
                m_buffer[i] = str[i] ^ static_cast<char>((m_key + i) * 71u);
            }
        }

        // 解密方法：运行时调用
        // 强制内联：防止生成独立的解密函数，增加逆向难度
#if defined(_MSC_VER)
        __forceinline
#else
        __attribute__((always_inline)) inline
#endif
        const char* decrypt() const {
            for (uint32_t i = 0; i < N; ++i) {
                m_buffer[i] ^= static_cast<char>((m_key + i) * 71u);
            }
            return m_buffer;
        }

        operator const char*() const {
            return decrypt();
        }
    };
}

#define HIDE(str) (StringGuard::Obfuscator<sizeof(str)>(str).decrypt())
