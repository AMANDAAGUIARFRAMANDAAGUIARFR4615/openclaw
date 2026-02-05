#pragma once

#include <cstdint>
#include <type_traits>

namespace DataGuard {

    // 编译期哈希：生成基于时间+行号的伪随机 Key
    constexpr uint32_t compileTimeHash(const char* str, uint32_t seed) {
        return *str ? compileTimeHash(str + 1, (seed ^ *str) * 16777619u) : seed;
    }

    // --- 字符串混淆 ---
    template <uint32_t N = 32>
    struct StrObfuscator {
        mutable char m_buffer[N];
        uint32_t m_key;

        template <uint32_t Len>
        consteval StrObfuscator(const char(&str)[Len]) : m_buffer{} {
            static_assert(Len <= N, "String literal is too long");
            constexpr int seconds = []{ constexpr auto t = __TIME__; return ((t[0]-'0')*10+(t[1]-'0'))*3600 + ((t[3]-'0')*10+(t[4]-'0'))*60 + ((t[6]-'0')*10+(t[7]-'0')); }();
            m_key = compileTimeHash(str, seconds);
            encrypt(str, Len);
        }

        template <uint32_t Len>
        constexpr StrObfuscator(const char(&str)[Len], uint32_t key) : m_buffer{}, m_key(key) {
            static_assert(Len <= N, "String literal is too long");
            encrypt(str, Len);
        }

        constexpr void encrypt(const char* str, uint32_t len) {
            for (uint32_t i = 0; i < len; ++i) {
                m_buffer[i] = str[i] ^ static_cast<char>((m_key + i) * 71u);
            }
        }

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

        operator const char*() const { return decrypt(); }
    };

    // --- 数值混淆 ---
    template <typename T>
    struct NumObfuscator {
        T m_val;
        uint32_t m_key;

        constexpr NumObfuscator(T val, uint32_t key) : m_val(val), m_key(key) {
            // 整数用异或，浮点数用加法混淆
            if constexpr (std::is_integral_v<T>) m_val ^= static_cast<T>(m_key);
            else m_val += static_cast<T>(m_key);
        }

#if defined(_MSC_VER)
        __forceinline
#else
        __attribute__((always_inline)) inline
#endif
        T decrypt() const {
            // 对应逆运算
            if constexpr (std::is_integral_v<T>) return m_val ^ static_cast<T>(m_key);
            else return m_val - static_cast<T>(m_key);
        }

        operator T() const { return decrypt(); }
    };
}

// 字符串保护宏
#define HIDE_STR(str) (DataGuard::StrObfuscator<sizeof(str)>(str, DataGuard::compileTimeHash(__TIME__, __LINE__)).decrypt())

// 数值保护宏
#define HIDE_NUM(val) (DataGuard::NumObfuscator<decltype(val)>(val, DataGuard::compileTimeHash(__TIME__, __LINE__)).decrypt())