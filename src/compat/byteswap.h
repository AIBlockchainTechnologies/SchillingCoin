// Copyright (c) 2014-present The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_COMPAT_BYTESWAP_H
#define SCHILLINGCOIN_COMPAT_BYTESWAP_H

#include <cstdint>

#ifdef _MSC_VER
#include <cstdlib>
#endif

// All internal_bswap_* functions can be replaced with std::byteswap once SCH
// requires C++23. Both libstdc++ and libc++ implement std::byteswap through
// the compiler builtins used here.
#ifndef DISABLE_BUILTIN_BSWAPS
#if defined(__has_builtin)
#if __has_builtin(__builtin_bswap16)
#define schillingcoin_builtin_bswap16(x) __builtin_bswap16(x)
#endif
#if __has_builtin(__builtin_bswap32)
#define schillingcoin_builtin_bswap32(x) __builtin_bswap32(x)
#endif
#if __has_builtin(__builtin_bswap64)
#define schillingcoin_builtin_bswap64(x) __builtin_bswap64(x)
#endif
#elif defined(_MSC_VER)
#define schillingcoin_builtin_bswap16(x) _byteswap_ushort(x)
#define schillingcoin_builtin_bswap32(x) _byteswap_ulong(x)
#define schillingcoin_builtin_bswap64(x) _byteswap_uint64(x)
#endif
#endif

// MSVC's _byteswap_* functions are not constexpr.
#ifndef _MSC_VER
#define BSWAP_CONSTEXPR constexpr
#else
#define BSWAP_CONSTEXPR
#endif

inline BSWAP_CONSTEXPR uint16_t internal_bswap_16(uint16_t x)
{
#ifdef schillingcoin_builtin_bswap16
    return schillingcoin_builtin_bswap16(x);
#else
    return (x >> 8) | (x << 8);
#endif
}

inline BSWAP_CONSTEXPR uint32_t internal_bswap_32(uint32_t x)
{
#ifdef schillingcoin_builtin_bswap32
    return schillingcoin_builtin_bswap32(x);
#else
    return (((x & 0xff000000U) >> 24) |
            ((x & 0x00ff0000U) >> 8) |
            ((x & 0x0000ff00U) << 8) |
            ((x & 0x000000ffU) << 24));
#endif
}

inline BSWAP_CONSTEXPR uint64_t internal_bswap_64(uint64_t x)
{
#ifdef schillingcoin_builtin_bswap64
    return schillingcoin_builtin_bswap64(x);
#else
    return (((x & 0xff00000000000000ULL) >> 56) |
            ((x & 0x00ff000000000000ULL) >> 40) |
            ((x & 0x0000ff0000000000ULL) >> 24) |
            ((x & 0x000000ff00000000ULL) >> 8) |
            ((x & 0x00000000ff000000ULL) << 8) |
            ((x & 0x0000000000ff0000ULL) << 24) |
            ((x & 0x000000000000ff00ULL) << 40) |
            ((x & 0x00000000000000ffULL) << 56));
#endif
}

#endif // SCHILLINGCOIN_COMPAT_BYTESWAP_H