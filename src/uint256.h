// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_UINT256_H
#define SCHILLINGCOIN_UINT256_H

#include <crypto/common.h>
#include <crypto/hex_base.h>
#include <span.h>
#include <util/strencodings.h>
#include <util/string.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <cstring>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>

/** Template base class for fixed-sized opaque blobs. */
template<unsigned int BITS>
class base_blob
{
protected:
    static constexpr int WIDTH = BITS / 8;
    static_assert(BITS % 8 == 0, "base_blob currently only supports whole bytes.");
    std::array<uint8_t, WIDTH> m_data;
    static_assert(WIDTH == sizeof(m_data), "Sanity check");

public:
    /* construct 0 value by default */
    constexpr base_blob() : m_data() {}

    /* constructor for constants between 1 and 255 */
    constexpr explicit base_blob(uint8_t v) : m_data{v} {}

    constexpr explicit base_blob(std::span<const unsigned char> vch)
    {
        assert(vch.size() == WIDTH);
        std::copy(vch.begin(), vch.end(), m_data.begin());
    }

    consteval explicit base_blob(std::string_view hex_str);

    constexpr bool IsNull() const
    {
        return std::all_of(m_data.begin(), m_data.end(), [](uint8_t val) {
            return val == 0;
        });
    }

    constexpr void SetNull()
    {
        std::fill(m_data.begin(), m_data.end(), 0);
    }

    constexpr bool operator==(const base_blob&) const = default;

    /** Lexicographic ordering
     * @note Does NOT match the ordering on the corresponding
     *       base_uint::CompareTo, which starts comparing from the end.
     */
    constexpr std::strong_ordering operator<=>(const base_blob& other) const = default;

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);
    std::string ToString() const;

    constexpr const unsigned char* data() const { return m_data.data(); }
    constexpr unsigned char* data() { return m_data.data(); }
    constexpr unsigned char* begin() { return m_data.data(); }
    constexpr unsigned char* end() { return m_data.data() + WIDTH; }
    constexpr const unsigned char* begin() const { return m_data.data(); }
    constexpr const unsigned char* end() const { return m_data.data() + WIDTH; }

    static constexpr unsigned int size() { return WIDTH; }

    constexpr uint64_t GetUint64(int pos) const { return ReadLE64(m_data.data() + pos * 8); }

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        s.write(MakeByteSpan(m_data));
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        s.read(MakeWritableByteSpan(m_data));
    }
};

template <unsigned int BITS>
consteval base_blob<BITS>::base_blob(std::string_view hex_str)
{
    if (hex_str.length() != m_data.size() * 2) throw "Hex string must fit exactly";
    auto str_it = hex_str.rbegin();
    for (auto& elem : m_data) {
        auto lo = util::ConstevalHexDigit(*(str_it++));
        elem = (util::ConstevalHexDigit(*(str_it++)) << 4) | lo;
    }
}

namespace detail {

/**
 * Writes the hex string in reverse byte order into a new uintN_t object.
 * A value is returned only when the input has the exact required length
 * and every input character is hexadecimal.
 */
template <class uintN_t>
std::optional<uintN_t> FromHex(std::string_view str)
{
    if (uintN_t::size() * 2 != str.size() || !IsHex(str)) return std::nullopt;

    uintN_t result;
    unsigned char* output = result.begin();
    unsigned char* output_end = result.end();
    size_t digits = str.size();

    while (digits > 0 && output < output_end) {
        *output = ::HexDigit(str[--digits]);
        if (digits > 0) {
            *output |= static_cast<unsigned char>(::HexDigit(str[--digits]) << 4);
            ++output;
        }
    }

    return result;
}

} // namespace detail

/** 160-bit opaque blob.
 * @note This type is called uint160 historical reasons only. It is an opaque
 * blob of 160 bits and has no integer operations.
 */
class uint160 : public base_blob<160> {
public:
    static std::optional<uint160> FromHex(std::string_view str) { return detail::FromHex<uint160>(str); }
    constexpr uint160() = default;
    constexpr explicit uint160(std::span<const unsigned char> vch) : base_blob<160>(vch) {}
};

/** 256-bit opaque blob.
 * @note This type is called uint256 for historical reasons only. It is an opaque
 * blob of 256 bits and has no integer operations. Use arith_uint256 if those are required.
 */
class uint256 : public base_blob<256> {
public:
    static std::optional<uint256> FromHex(std::string_view str) { return detail::FromHex<uint256>(str); }
    constexpr uint256() = default;
    consteval explicit uint256(std::string_view hex_str) : base_blob<256>(hex_str) {}
    constexpr explicit uint256(uint8_t v) : base_blob<256>(v) {}
    constexpr explicit uint256(std::span<const unsigned char> vch) : base_blob<256>(vch) {}

    uint64_t GetCheapHash() const { return ReadLE64(begin()); }

    static const uint256 ZERO;
    static const uint256 ONE;
    static const uint256 TWO;
};

/* uint256 from const char *.
 * This is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching uint256(0).
 */
inline uint256 uint256S(const char *str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}

/* uint256 from std::string.
 * This is a separate function because the constructor uint256(const std::string &str) can result
 * in dangerously catching uint256(0) via std::string(const char*).
 */
inline uint256 uint256S(const std::string& str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}

/** 512-bit unsigned big integer. */
class uint512 : public base_blob<512> {
public:
    constexpr uint512() = default;
    constexpr uint512(const base_blob<512>& b) : base_blob<512>(b) {}
    constexpr explicit uint512(std::span<const unsigned char> vch) : base_blob<512>(vch) {}

    uint256 trim256() const
    {
        uint256 result;
        memcpy((void*)&result, (void*)m_data.data(), 32);
        return result;
    }
};

/* uint512 from const char *.
 * This is a separate function because the constructor uint512(const char*) can result
 * in dangerously catching uint512(0).
 */
inline uint512 uint512S(const char* str)
{
    uint512 rv;
    rv.SetHex(str);
    return rv;
}

/* uint512 from std::string.
 * This is a separate function because the constructor uint512(const std::string &str) can result
 * in dangerously catching uint512(0) via std::string(const char*).
 */
inline uint512 uint512S(const std::string& str)
{
    uint512 rv;
    rv.SetHex(str);
    return rv;
}

/** constant uint256 instances */
const uint256 UINT256_ZERO = uint256();
const uint256 UINT256_ONE = uint256S("0000000000000000000000000000000000000000000000000000000000000001");

#endif // SCHILLINGCOIN_UINT256_H