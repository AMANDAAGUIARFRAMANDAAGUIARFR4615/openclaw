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

    template <uint32_t N = 32>
    struct Obfuscator {
        // 使用 mutable 允许在 const 对象中解密 (用于隐式转换)
        mutable char m_buffer[N];
        uint32_t m_key;

        template <uint32_t Len>
        consteval Obfuscator(const char(&str)[Len]) : m_buffer{}
        {
            static_assert(Len <= N, "String literal is too long for StringGuard::Obfuscator");
            m_key = compileTimeHash(str, __TIME__);
            encrypt(str, Len);
        }

        template <uint32_t Len>
        constexpr Obfuscator(const char(&str)[Len], uint32_t key) : m_buffer{}, m_key(key)
        {
             static_assert(Len <= N, "String literal is too long");
             encrypt(str, Len);
        }

        constexpr void encrypt(const char* str, uint32_t len) {
            for (uint32_t i = 0; i < len; ++i) {
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

#define HIDE(str) (StringGuard::Obfuscator<sizeof(str)>(str, StringGuard::getSeed(__LINE__)).decrypt())