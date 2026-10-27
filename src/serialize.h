// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_SERIALIZE_H
#define SCHILLINGCOIN_SERIALIZE_H

#include <algorithm>
#include <assert.h>
#include <ios>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <string.h>
#include <type_traits>
#include <utility>
#include <vector>

#include "optional.h"
#include "stubs/legacy_zerocoin_consensus.h"

class CScript;

static const unsigned int MAX_SIZE = 0x02000000;

/**
 * Used to bypass the rule against non-const reference to temporary
 * where it makes sense with wrappers such as CFlatData or CTxDB
 */
template <typename T>
inline T& REF(const T& val)
{
    return const_cast<T&>(val);
}

/**
 * Used to acquire a non-const pointer "this" to generate bodies
 * of const serialization operations from a template
 */
template <typename T>
inline T* NCONST_PTR(const T* val)
{
    return const_cast<T*>(val);
}

/////////////////////////////////////////////////////////////////
//
// Templates for serializing to anything that looks like a stream,
// i.e. anything that supports .read(char*, size_t) and .write(char*, size_t)
//

enum {
    // primary actions
    SER_NETWORK = (1 << 0),
    SER_DISK = (1 << 1),
    SER_GETHASH = (1 << 2),
};

/**
 * Convert any argument to a reference to a base class while preserving
 * constness. This allows base-class serialization from SERIALIZE_METHODS.
 */
template <class Out, class In>
Out& AsBase(In& value)
{
    static_assert(std::is_base_of_v<Out, In>);
    return value;
}

template <class Out, class In>
const Out& AsBase(const In& value)
{
    static_assert(std::is_base_of_v<Out, In>);
    return value;
}

#define READWRITE(...) (::SerReadWriteMany(s, ser_action, __VA_ARGS__))

#define READWRITEAS(type, obj) \
    (::SerReadWriteMany(s, ser_action, AsBase<type>(obj)))

#define SER_READ(obj, code) \
    ::SerRead( \
        s, \
        ser_action, \
        obj, \
        [&](Stream& s, std::remove_const_t<Type>& obj) { code; })

#define SER_WRITE(obj, code) \
    ::SerWrite( \
        s, \
        ser_action, \
        obj, \
        [&](Stream& s, const Type& obj) { code; })

#define FORMATTER_METHODS(cls, obj)                                      \
    template <typename Stream>                                          \
    static void Ser(Stream& s, const cls& obj)                           \
    {                                                                   \
        SerializationOps(obj, s, CSerActionSerialize{});                \
    }                                                                   \
    template <typename Stream>                                          \
    static void Unser(Stream& s, cls& obj)                              \
    {                                                                   \
        SerializationOps(obj, s, CSerActionUnserialize{});              \
    }                                                                   \
    template <typename Stream, typename Type, typename Operation>       \
    static void SerializationOps(Type& obj, Stream& s, Operation ser_action)

#define BASE_SERIALIZE_METHODS(cls)                                      \
    template <typename Stream>                                          \
    void Serialize(Stream& s) const                                     \
    {                                                                   \
        static_assert(                                                  \
            std::is_same_v<const cls&, decltype(*this)>,                \
            "Serialize type mismatch");                                 \
        Ser(s, *this);                                                  \
    }                                                                   \
    template <typename Stream>                                          \
    void Unserialize(Stream& s)                                         \
    {                                                                   \
        static_assert(                                                  \
            std::is_same_v<cls&, decltype(*this)>,                      \
            "Unserialize type mismatch");                               \
        Unser(s, *this);                                                \
    }

#define SERIALIZE_METHODS(cls, obj) \
    BASE_SERIALIZE_METHODS(cls)     \
    FORMATTER_METHODS(cls, obj)

/**
 * Serialization is implemented through SERIALIZE_METHODS.
 */

/*
 * Basic Types
 */
#define WRITEDATA(s, obj) \
    s.write(reinterpret_cast<const char*>(&(obj)), sizeof(obj))
#define READDATA(s, obj) \
    s.read(reinterpret_cast<char*>(&(obj)), sizeof(obj))

template <typename Stream>
inline void Serialize(Stream& s, char a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, signed char a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, unsigned char a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, signed short a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, unsigned short a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, signed int a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, unsigned int a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, signed long a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, unsigned long a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, signed long long a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, unsigned long long a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, float a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, double a)
{
    WRITEDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, char& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, signed char& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, unsigned char& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, signed short& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, unsigned short& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, signed int& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, unsigned int& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, signed long& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, unsigned long& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, signed long long& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, unsigned long long& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, float& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Unserialize(Stream& s, double& a)
{
    READDATA(s, a);
}

template <typename Stream>
inline void Serialize(Stream& s, bool a)
{
    char f = a;
    WRITEDATA(s, f);
}

template <typename Stream>
inline void Unserialize(Stream& s, bool& a)
{
    char f;
    READDATA(s, f);
    a = f;
}

// Serialization for libzerocoin::CoinDenomination
template <typename Stream>
inline void Serialize(Stream& s, libzerocoin::CoinDenomination a)
{
    int f = libzerocoin::ZerocoinDenominationToInt(a);
    WRITEDATA(s, f);
}

template <typename Stream>
inline void Unserialize(Stream& s, libzerocoin::CoinDenomination& a)
{
    int f = 0;
    READDATA(s, f);
    a = libzerocoin::IntToZerocoinDenomination(f);
}

// Serialization for libzerocoin::SpendType
template <typename Stream>
inline void Serialize(Stream& s, libzerocoin::SpendType a)
{
    uint8_t f = static_cast<uint8_t>(a);
    WRITEDATA(s, f);
}

template <typename Stream>
inline void Unserialize(Stream& s, libzerocoin::SpendType& a)
{
    uint8_t f = 0;
    READDATA(s, f);
    a = static_cast<libzerocoin::SpendType>(f);
}

/**
 * Compact Size
 * size <  253        -- 1 byte
 * size <= USHRT_MAX  -- 3 bytes  (253 + 2 bytes)
 * size <= UINT_MAX   -- 5 bytes  (254 + 4 bytes)
 * size >  UINT_MAX   -- 9 bytes  (255 + 8 bytes)
 */
inline unsigned int GetSizeOfCompactSize(uint64_t nSize)
{
    if (nSize < 253)
        return sizeof(unsigned char);
    else if (nSize <= std::numeric_limits<unsigned short>::max())
        return sizeof(unsigned char) + sizeof(unsigned short);
    else if (nSize <= std::numeric_limits<unsigned int>::max())
        return sizeof(unsigned char) + sizeof(unsigned int);
    else
        return sizeof(unsigned char) + sizeof(uint64_t);
}

template <typename Stream>
void WriteCompactSize(Stream& os, uint64_t nSize)
{
    if (nSize < 253) {
        unsigned char chSize = nSize;
        WRITEDATA(os, chSize);
    } else if (nSize <= std::numeric_limits<unsigned short>::max()) {
        unsigned char chSize = 253;
        unsigned short xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    } else if (nSize <= std::numeric_limits<unsigned int>::max()) {
        unsigned char chSize = 254;
        unsigned int xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    } else {
        unsigned char chSize = 255;
        uint64_t xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    return;
}

template <typename Stream>
uint64_t ReadCompactSize(Stream& is)
{
    unsigned char chSize;
    READDATA(is, chSize);
    uint64_t nSizeRet = 0;
    if (chSize < 253) {
        nSizeRet = chSize;
    } else if (chSize == 253) {
        unsigned short xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
        if (nSizeRet < 253)
            throw std::ios_base::failure("non-canonical ReadCompactSize()");
    } else if (chSize == 254) {
        unsigned int xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
        if (nSizeRet < 0x10000u)
            throw std::ios_base::failure("non-canonical ReadCompactSize()");
    } else {
        uint64_t xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
        if (nSizeRet < 0x100000000ULL)
            throw std::ios_base::failure("non-canonical ReadCompactSize()");
    }
    if (nSizeRet > (uint64_t)MAX_SIZE)
        throw std::ios_base::failure("ReadCompactSize() : size too large");
    return nSizeRet;
}

/**
 * Variable-length integers: bytes are a MSB base-128 encoding of the number.
 * The high bit in each byte signifies whether another digit follows. To make
 * sure the encoding is one-to-one, one is subtracted from all but the last digit.
 * Thus, the byte sequence a[] with length len, where all but the last byte
 * has bit 128 set, encodes the number:
 *
 *  (a[len-1] & 0x7F) + sum(i=1..len-1, 128^i*((a[len-i-1] & 0x7F)+1))
 *
 * Properties:
 * * Very small (0-127: 1 byte, 128-16511: 2 bytes, 16512-2113663: 3 bytes)
 * * Every integer has exactly one encoding
 * * Encoding does not depend on size of original integer type
 * * No redundancy: every (infinite) byte sequence corresponds to a list
 *   of encoded integers.
 *
 * 0:         [0x00]  256:        [0x81 0x00]
 * 1:         [0x01]  16383:      [0xFE 0x7F]
 * 127:       [0x7F]  16384:      [0xFF 0x00]
 * 128:  [0x80 0x00]  16511: [0x80 0xFF 0x7F]
 * 255:  [0x80 0x7F]  65535: [0x82 0xFD 0x7F]
 * 2^32:           [0x8E 0xFE 0xFE 0xFF 0x00]
 */

template <typename I>
inline unsigned int GetSizeOfVarInt(I n)
{
    int nRet = 0;
    while (true) {
        nRet++;
        if (n <= 0x7F)
            break;
        n = (n >> 7) - 1;
    }
    return nRet;
}

template <typename Stream, typename I>
void WriteVarInt(Stream& os, I n)
{
    unsigned char tmp[(sizeof(n) * 8 + 6) / 7];
    int len = 0;
    while (true) {
        tmp[len] = (n & 0x7F) | (len ? 0x80 : 0x00);
        if (n <= 0x7F)
            break;
        n = (n >> 7) - 1;
        len++;
    }
    do {
        WRITEDATA(os, tmp[len]);
    } while (len--);
}

template <typename Stream, typename I>
I ReadVarInt(Stream& is)
{
    I n = 0;
    while (true) {
        unsigned char chData;
        READDATA(is, chData);
        n = (n << 7) | (chData & 0x7F);
        if (chData & 0x80)
            n++;
        else
            return n;
    }
}

#define FLATDATA(obj) REF(CFlatData((char*)&(obj), (char*)&(obj) + sizeof(obj)))
#define VARINT(obj) REF(WrapVarInt(REF(obj)))
#define COMPACTSIZE(obj) REF(CCompactSize(REF(obj)))
#define LIMITED_STRING(obj, n) REF(LimitedString<n>(REF(obj)))

/**
 * Wrapper for serializing arrays and POD.
 */
class CFlatData
{
protected:
    char* pbegin;
    char* pend;

public:
    CFlatData(void* pbeginIn, void* pendIn)
        : pbegin((char*)pbeginIn),
          pend((char*)pendIn)
    {
    }

    template <class T, class TAl>
    explicit CFlatData(std::vector<T, TAl>& v)
        : pbegin((char*)v.data()),
          pend((char*)(v.data() + v.size()))
    {
    }

    char* begin() { return pbegin; }
    const char* begin() const { return pbegin; }
    char* end() { return pend; }
    const char* end() const { return pend; }

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        s.write(pbegin, pend - pbegin);
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        s.read(pbegin, pend - pbegin);
    }
};

template <typename I>
class CVarInt
{
protected:
    I& n;

public:
    explicit CVarInt(I& nIn)
        : n(nIn)
    {
    }

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        WriteVarInt<Stream, I>(s, n);
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        n = ReadVarInt<Stream, I>(s);
    }
};

class CCompactSize
{
protected:
    uint64_t& n;

public:
    explicit CCompactSize(uint64_t& nIn)
        : n(nIn)
    {
    }

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        WriteCompactSize(s, n);
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        n = ReadCompactSize(s);
    }
};

template <size_t Limit>
class LimitedString
{
protected:
    std::string& string;

public:
    explicit LimitedString(std::string& stringIn)
        : string(stringIn)
    {
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        const size_t size = ReadCompactSize(s);

        if (size > Limit) {
            throw std::ios_base::failure("String length limit exceeded");
        }

        string.resize(size);

        if (size != 0)
            s.read(&string[0], size);
    }

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        WriteCompactSize(s, string.size());

        if (!string.empty())
            s.write(string.data(), string.size());
    }
};

template <typename I>
CVarInt<I> WrapVarInt(I& n)
{
    return CVarInt<I>(n);
}

/**
 * Wrapper for serializing an object through a formatter.
 */
template <typename Formatter, typename T>
class Wrapper
{
    static_assert(
        std::is_lvalue_reference_v<T>,
        "Wrapper requires an lvalue reference");

private:
    T object;

public:
    explicit Wrapper(T objectIn)
        : object(objectIn)
    {
    }

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        Formatter().Ser(s, object);
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        Formatter().Unser(s, object);
    }
};

/**
 * Serialize or unserialize an object through the specified formatter.
 */
template <typename Formatter, typename T>
inline Wrapper<Formatter, T&> Using(T&& object)
{
    return Wrapper<Formatter, T&>(object);
}

/**
 * Default formatter. Serializes objects through normal dispatch.
 */
struct DefaultFormatter
{
    template <typename Stream, typename T>
    static void Ser(Stream& s, const T& object)
    {
        ::Serialize(s, object);
    }

    template <typename Stream, typename T>
    static void Unser(Stream& s, T& object)
    {
        ::Unserialize(s, object);
    }
};

/**
 * Serialize or unserialize vector elements through another formatter.
 */
template <typename Formatter>
struct VectorFormatter
{
    template <typename Stream, typename V>
    void Ser(Stream& s, const V& values)
    {
        Formatter formatter;

        WriteCompactSize(s, values.size());

        for (const typename V::value_type& value : values) {
            formatter.Ser(s, value);
        }
    }

    template <typename Stream, typename V>
    void Unser(Stream& s, V& values)
    {
        Formatter formatter;

        values.clear();

        const size_t size = ReadCompactSize(s);
        size_t allocated = 0;

        while (allocated < size) {
            static_assert(
                sizeof(typename V::value_type) <= 5000000,
                "Vector element size is too large");

            const size_t batch =
                std::max<size_t>(
                    1,
                    5000000 / sizeof(typename V::value_type));

            allocated = std::min(size, allocated + batch);
            values.reserve(allocated);

            while (values.size() < allocated) {
                values.emplace_back();
                formatter.Unser(s, values.back());
            }
        }
    }
};

/**
 * Forward declarations
 */

/**
 *  std::string
 */
template <typename Stream, typename C>
void Serialize(Stream& os, const std::basic_string<C>& str);

template <typename Stream, typename C>
void Unserialize(Stream& is, std::basic_string<C>& str);

/**
 * optional
 */
template <typename Stream, typename T>
void Serialize(Stream& os, const Optional<T>& item);

template <typename Stream, typename T>
void Unserialize(Stream& is, Optional<T>& item);

/**
 * vector
 * vectors of unsigned char are a special case and are intended to be serialized as a single opaque blob.
 */
template <typename Stream, typename T, typename A>
void Serialize_impl(Stream& os, const std::vector<T, A>& v, const unsigned char&);

template <typename Stream, typename T, typename A, typename V>
void Serialize_impl(Stream& os, const std::vector<T, A>& v, const V&);

template <typename Stream, typename T, typename A>
inline void Serialize(Stream& os, const std::vector<T, A>& v);

template <typename Stream, typename T, typename A>
void Unserialize_impl(Stream& is, std::vector<T, A>& v, const unsigned char&);

template <typename Stream, typename T, typename A, typename V>
void Unserialize_impl(Stream& is, std::vector<T, A>& v, const V&);

template <typename Stream, typename T, typename A>
inline void Unserialize(Stream& is, std::vector<T, A>& v);

/**
 * pair
 */
template <typename Stream, typename K, typename T>
void Serialize(Stream& os, const std::pair<K, T>& item);

template <typename Stream, typename K, typename T>
void Unserialize(Stream& is, std::pair<K, T>& item);

/**
 * map
 */
template <typename Stream, typename K, typename T, typename Pred, typename A>
void Serialize(Stream& os, const std::map<K, T, Pred, A>& m);

template <typename Stream, typename K, typename T, typename Pred, typename A>
void Unserialize(Stream& is, std::map<K, T, Pred, A>& m);

/**
 * set
 */
template <typename Stream, typename K, typename Pred, typename A>
void Serialize(Stream& os, const std::set<K, Pred, A>& m);

template <typename Stream, typename K, typename Pred, typename A>
void Unserialize(Stream& is, std::set<K, Pred, A>& m);


/**
 * If none of the specialized versions above matched, default to calling
 * the object's stream-owned member serialization function.
 */
template <typename Stream, typename T>
inline void Serialize(Stream& os, const T& a)
{
    a.Serialize(os);
}

template <typename Stream, typename T>
inline void Unserialize(Stream& is, T&& a)
{
    a.Unserialize(is);
}

/**
 * string
 */
template <typename Stream, typename C>
void Serialize(Stream& os, const std::basic_string<C>& str)
{
    WriteCompactSize(os, str.size());

    if (!str.empty()) {
        os.write(
            reinterpret_cast<const char*>(str.data()),
            str.size() * sizeof(C));
    }
}

template <typename Stream, typename C>
void Unserialize(Stream& is, std::basic_string<C>& str)
{
    const unsigned int nSize = ReadCompactSize(is);
    str.resize(nSize);

    if (nSize != 0) {
        is.read(
            reinterpret_cast<char*>(str.data()),
            nSize * sizeof(C));
    }
}

/**
 * vector
 */
template <typename Stream, typename T, typename A>
void Serialize_impl(Stream& os, const std::vector<T, A>& v, const unsigned char&)
{
    WriteCompactSize(os, v.size());

    if (!v.empty()) {
        os.write(
            reinterpret_cast<const char*>(v.data()),
            v.size() * sizeof(T));
    }
}

template <typename Stream, typename T, typename A>
void Serialize_impl(Stream& os, const std::vector<T, A>& v, const bool&)
{
    // std::vector<bool> uses proxy references rather than actual bool references.
    WriteCompactSize(os, v.size());

    for (bool value : v)
        ::Serialize(os, value);
}

template <typename Stream, typename T, typename A, typename V>
void Serialize_impl(Stream& os, const std::vector<T, A>& v, const V&)
{
    WriteCompactSize(os, v.size());

    for (const T& value : v)
        ::Serialize(os, value);
}

template <typename Stream, typename T, typename A>
inline void Serialize(Stream& os, const std::vector<T, A>& v)
{
    Serialize_impl(os, v, T());
}

template <typename Stream, typename T, typename A>
void Unserialize_impl(Stream& is, std::vector<T, A>& v, const unsigned char&)
{
    // Limit size per read so bogus size value won't cause out of memory
    v.clear();

    const unsigned int nSize = ReadCompactSize(is);
    unsigned int i = 0;

    while (i < nSize) {
        const unsigned int blk =
            std::min(nSize - i, (unsigned int)(1 + 4999999 / sizeof(T)));

        v.resize(i + blk);
        is.read(
            reinterpret_cast<char*>(&v[i]),
            blk * sizeof(T));
        i += blk;
    }
}

template <typename Stream, typename T, typename A, typename V>
void Unserialize_impl(Stream& is, std::vector<T, A>& v, const V&)
{
    v.clear();

    const unsigned int nSize = ReadCompactSize(is);
    unsigned int i = 0;
    unsigned int nMid = 0;

    while (nMid < nSize) {
        nMid += 5000000 / sizeof(T);

        if (nMid > nSize)
            nMid = nSize;

        v.resize(nMid);

        for (; i < nMid; ++i)
            ::Unserialize(is, v[i]);
    }
}

template <typename Stream, typename T, typename A>
inline void Unserialize(Stream& is, std::vector<T, A>& v)
{
    Unserialize_impl(is, v, T());
}

/**
 * optional
 */
template <typename Stream, typename T>
void Serialize(Stream& os, const Optional<T>& item)
{
    const unsigned char discriminant = item ? 0x01 : 0x00;
    Serialize(os, discriminant);

    if (item)
        Serialize(os, *item);
}

template <typename Stream, typename T>
void Unserialize(Stream& is, Optional<T>& item)
{
    unsigned char discriminant = 0x00;
    Unserialize(is, discriminant);

    if (discriminant == 0x00) {
        item = nullopt;
        return;
    }

    T object;
    Unserialize(is, object);
    item = object;
}

/**
 * pair
 */
template <typename Stream, typename K, typename T>
void Serialize(Stream& os, const std::pair<K, T>& item)
{
    Serialize(os, item.first);
    Serialize(os, item.second);
}

template <typename Stream, typename K, typename T>
void Unserialize(Stream& is, std::pair<K, T>& item)
{
    Unserialize(is, item.first);
    Unserialize(is, item.second);
}

/**
 * map
 */
template <typename Stream, typename K, typename T, typename Pred, typename A>
void Serialize(Stream& os, const std::map<K, T, Pred, A>& m)
{
    WriteCompactSize(os, m.size());

    for (const auto& entry : m)
        Serialize(os, entry);
}

template <typename Stream, typename K, typename T, typename Pred, typename A>
void Unserialize(Stream& is, std::map<K, T, Pred, A>& m)
{
    m.clear();

    const unsigned int nSize = ReadCompactSize(is);
    typename std::map<K, T, Pred, A>::iterator mi = m.begin();

    for (unsigned int i = 0; i < nSize; ++i) {
        std::pair<K, T> item;
        Unserialize(is, item);
        mi = m.insert(mi, item);
    }
}

/**
 * set
 */
template <typename Stream, typename K, typename Pred, typename A>
void Serialize(Stream& os, const std::set<K, Pred, A>& m)
{
    WriteCompactSize(os, m.size());

    for (const K& key : m)
        Serialize(os, key);
}

template <typename Stream, typename K, typename Pred, typename A>
void Unserialize(Stream& is, std::set<K, Pred, A>& m)
{
    m.clear();

    const unsigned int nSize = ReadCompactSize(is);
    typename std::set<K, Pred, A>::iterator it = m.begin();

    for (unsigned int i = 0; i < nSize; ++i) {
        K key;
        Unserialize(is, key);
        it = m.insert(it, key);
    }
}

/**
 * Support for SERIALIZE_METHODS and READWRITE.
 */
struct CSerActionSerialize
{
    constexpr bool ForRead() const
    {
        return false;
    }
};

struct CSerActionUnserialize
{
    constexpr bool ForRead() const
    {
        return true;
    }
};

/**
 * Stream used to compute serialized sizes.
 */
class CSizeComputer
{
private:
    size_t nSize;
    const int nType;
    const int nVersion;

public:
    CSizeComputer(int nTypeIn, int nVersionIn)
        : nSize(0),
          nType(nTypeIn),
          nVersion(nVersionIn)
    {
    }

    explicit CSizeComputer(int nVersionIn = 0)
        : nSize(0),
          nType(0),
          nVersion(nVersionIn)
    {
    }

    CSizeComputer& write(const char*, size_t size)
    {
        nSize += size;
        return *this;
    }

    void seek(size_t size)
    {
        nSize += size;
    }

    template <typename T>
    CSizeComputer& operator<<(const T& obj)
    {
        ::Serialize(*this, obj);
        return *this;
    }

    size_t size() const
    {
        return nSize;
    }

    int GetType() const
    {
        return nType;
    }

    int GetVersion() const
    {
        return nVersion;
    }
};

/**
 * Serialize and unserialize multiple objects through stream-owned dispatch.
 */
template <typename Stream>
inline void SerializeMany(Stream&)
{
}

template <typename Stream, typename Arg, typename... Args>
inline void SerializeMany(
    Stream& s,
    const Arg& arg,
    const Args&... args)
{
    ::Serialize(s, arg);
    ::SerializeMany(s, args...);
}

template <typename Stream>
inline void UnserializeMany(Stream&)
{
}

template <typename Stream, typename Arg, typename... Args>
inline void UnserializeMany(
    Stream& s,
    Arg&& arg,
    Args&&... args)
{
    ::Unserialize(s, arg);
    ::UnserializeMany(s, args...);
}

template <typename Stream, typename... Args>
inline void SerReadWriteMany(
    Stream& s,
    CSerActionSerialize,
    const Args&... args)
{
    ::SerializeMany(s, args...);
}

template <typename Stream, typename... Args>
inline void SerReadWriteMany(
    Stream& s,
    CSerActionUnserialize,
    Args&&... args)
{
    ::UnserializeMany(s, args...);
}

template <typename Stream, typename Type, typename Fn>
inline void SerRead(
    Stream&,
    CSerActionSerialize,
    Type&&,
    Fn&&)
{
}

template <typename Stream, typename Type, typename Fn>
inline void SerRead(
    Stream& s,
    CSerActionUnserialize,
    Type&& object,
    Fn&& function)
{
    function(s, std::forward<Type>(object));
}

template <typename Stream, typename Type, typename Fn>
inline void SerWrite(
    Stream& s,
    CSerActionSerialize,
    Type&& object,
    Fn&& function)
{
    function(s, std::forward<Type>(object));
}

template <typename Stream, typename Type, typename Fn>
inline void SerWrite(
    Stream&,
    CSerActionUnserialize,
    Type&&,
    Fn&&)
{
}

/**
 * Compatibility adapters for existing SCH callers.
 */
template <typename Stream, typename... Args>
inline void SerializeMany(Stream& s, int, int, const Args&... args)
{
    ::SerializeMany(s, args...);
}

template <typename Stream, typename... Args>
inline void UnserializeMany(Stream& s, int, int, Args&&... args)
{
    ::UnserializeMany(s, std::forward<Args>(args)...);
}

/**
 * CSizeComputer specializations.
 */
template <typename I>
inline void WriteVarInt(CSizeComputer& s, I n)
{
    s.seek(GetSizeOfVarInt<I>(n));
}

inline void WriteCompactSize(CSizeComputer& s, uint64_t nSize)
{
    s.seek(GetSizeOfCompactSize(nSize));
}

/**
 * Serialized-size helpers.
 */
template <typename T>
size_t GetSerializeSize(const T& obj, int nVersion = 0)
{
    return (CSizeComputer(nVersion) << obj).size();
}

/**
 * Compatibility adapter for existing SCH callers using
 * GetSerializeSize(object, type, version).
 */
template <typename T>
size_t GetSerializeSize(const T& obj, int nType, int nVersion)
{
    return (CSizeComputer(nType, nVersion) << obj).size();
}

template <typename... T>
size_t GetSerializeSizeMany(int nVersion, const T&... objects)
{
    CSizeComputer sizeComputer(nVersion);
    SerializeMany(sizeComputer, objects...);
    return sizeComputer.size();
}

#endif // SCHILLINGCOIN_SERIALIZE_H