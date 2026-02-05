#pragma once

#include <cstdint>
#include <type_traits>
#include <bit>
#include <array>

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

        // 使用 consteval 强制编译期执行 (C++20)
        template <uint32_t Len>
        consteval StrObfuscator(const char(&str)[Len]) : m_buffer{} {
            static_assert(Len <= N, "String literal is too long");
            constexpr int seconds = []{ 
                constexpr auto t = __TIME__; 
                return ((t[0]-'0')*10+(t[1]-'0'))*3600 + ((t[3]-'0')*10+(t[4]-'0'))*60 + ((t[6]-'0')*10+(t[7]-'0')); 
            }();
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
    // 逻辑：将任何数值类型(int/float/double)视为字节数组，
    // 使用与 StrObfuscator 完全相同的滚动异或算法进行混淆。
    template <typename T>
    struct NumObfuscator {
        mutable uint8_t m_buffer[sizeof(T)];
        uint32_t m_key;

        consteval NumObfuscator(T val, uint32_t key) : m_buffer{}, m_key(key) {
            // 将数值(无论是float还是int)转换为纯二进制字节数组
            // std::bit_cast 是 C++20 中唯一能在 constexpr/consteval 上下文中进行类型这一转换的标准方法
            auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(val);

            for (size_t i = 0; i < sizeof(T); ++i) {
                m_buffer[i] = bytes[i] ^ static_cast<uint8_t>((m_key + i) * 71u);
            }
        }

#if defined(_MSC_VER)
        __forceinline
#else
        __attribute__((always_inline)) inline
#endif
        T decrypt() const {
            for (size_t i = 0; i < sizeof(T); ++i) {
                m_buffer[i] ^= static_cast<uint8_t>((m_key + i) * 71u);
            }
            
            return std::bit_cast<T>(m_buffer);
        }

        // 隐式转换支持
        operator T() const { return decrypt(); }
    };
}

// 字符串保护宏
#define HIDE_STR(str) (DataGuard::StrObfuscator<sizeof(str)>(str, DataGuard::compileTimeHash(__TIME__, __LINE__)).decrypt())

// 数值保护宏
#define HIDE_NUM(val) (DataGuard::NumObfuscator<std::remove_cv_t<decltype(val)>>(val, DataGuard::compileTimeHash(__TIME__, __LINE__)).decrypt())