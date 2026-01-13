#pragma once

#include <cstddef>
#include <cstdint>

namespace StringGuard {

    // 编译期哈希：生成基于时间+行号的伪随机 Key
    constexpr uint32_t compileTimeHash(const char* str, uint32_t seed) {
        return *str ? compileTimeHash(str + 1, (seed ^ *str) * 16777619u) : seed;
    }

    constexpr uint32_t getSeed(int line) {
        return compileTimeHash(__TIME__, line);
    }

    template <size_t N, uint32_t Key>
    struct Obfuscator {
        char m_buffer[N];

        // 构造函数：编译期加密
        constexpr Obfuscator(const char(&str)[N]) : m_buffer{} {
            for (size_t i = 0; i < N; ++i) {
                // 简单的混合算法：(Key + Index) ^ Char
                m_buffer[i] = str[i] ^ static_cast<char>((Key + i) * 71u);
            }
        }

        // 解密方法：运行时调用
        // 强制内联：防止生成独立的解密函数，增加逆向难度
#if defined(_MSC_VER)
        __forceinline
#else
        __attribute__((always_inline)) inline
#endif
        const char* decrypt() {
            for (size_t i = 0; i < N; ++i) {
                m_buffer[i] ^= static_cast<char>((Key + i) * 71u);
            }
            return m_buffer;
        }
    };
}

#define HIDE(str) (StringGuard::Obfuscator<sizeof(str), StringGuard::getSeed(__LINE__)>(str).decrypt())
