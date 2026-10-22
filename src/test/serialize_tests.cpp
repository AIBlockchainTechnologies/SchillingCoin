// Copyright (c) 2012-2013 The Bitcoin Core developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.h"
#include "optional.h"
#include "primitives/transaction.h"
#include "serialize.h"
#include "streams.h"
#include "test/test_schillingcoin.h"

#include <stdint.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(serialize_tests, BasicTestingSetup)

template<typename T>
void check_ser_rep(T thing, std::vector<unsigned char> expected)
{
    CDataStream ss(SER_DISK, 0);
    ss << thing;

    BOOST_CHECK(GetSerializeSize(thing, 0, 0) == ss.size());

    std::vector<unsigned char> serialized_representation(ss.begin(), ss.end());

    BOOST_CHECK(serialized_representation == expected);

    T thing_deserialized;
    ss >> thing_deserialized;

    BOOST_CHECK(thing_deserialized == thing);
}

BOOST_AUTO_TEST_CASE(boost_optional)
{
    check_ser_rep<Optional<unsigned char> >(0xff, {0x01, 0xff});
    check_ser_rep<Optional<unsigned char> >(nullopt, {0x00});
    check_ser_rep<Optional<std::string> >(std::string("Test"),
                                          {0x01, 0x04, 'T', 'e', 's', 't'});
}

class CReadWriteManyTest
{
public:
    uint8_t first;
    uint16_t second;
    std::string third;

    CReadWriteManyTest()
        : first(0),
          second(0),
          third()
    {
    }

    CReadWriteManyTest(uint8_t firstIn, uint16_t secondIn, const std::string& thirdIn)
        : first(firstIn),
          second(secondIn),
          third(thirdIn)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITEMANY(first, second, third);
    }

    bool operator==(const CReadWriteManyTest& other) const
    {
        return first == other.first &&
               second == other.second &&
               third == other.third;
    }
};

BOOST_AUTO_TEST_CASE(serialize_many)
{
    const uint8_t first = 0x12;
    const uint16_t second = 0x3456;
    const std::string third = "SCH";

    CDataStream manyStream(SER_DISK, 0);
    SerializeMany(manyStream, SER_DISK, 0, first, second, third);

    CDataStream legacyStream(SER_DISK, 0);
    legacyStream << first << second << third;

    const std::vector<unsigned char> manyBytes(
        manyStream.begin(),
        manyStream.end());

    const std::vector<unsigned char> legacyBytes(
        legacyStream.begin(),
        legacyStream.end());

    BOOST_CHECK(manyBytes == legacyBytes);

    uint8_t firstOut = 0;
    uint16_t secondOut = 0;
    std::string thirdOut;

    UnserializeMany(
        manyStream,
        SER_DISK,
        0,
        firstOut,
        secondOut,
        thirdOut);

    BOOST_CHECK_EQUAL(firstOut, first);
    BOOST_CHECK_EQUAL(secondOut, second);
    BOOST_CHECK_EQUAL(thirdOut, third);
    BOOST_CHECK(manyStream.empty());
}

BOOST_AUTO_TEST_CASE(readwrite_many_and_size_computer)
{
    const CReadWriteManyTest original(0x12, 0x3456, "SCH");

    CDataStream readWriteManyStream(SER_DISK, 0);
    readWriteManyStream << original;

    CDataStream legacyStream(SER_DISK, 0);
    legacyStream << original.first
                 << original.second
                 << original.third;

    const std::vector<unsigned char> readWriteManyBytes(
        readWriteManyStream.begin(),
        readWriteManyStream.end());

    const std::vector<unsigned char> legacyBytes(
        legacyStream.begin(),
        legacyStream.end());

    BOOST_CHECK(readWriteManyBytes == legacyBytes);

    CReadWriteManyTest decoded;
    readWriteManyStream >> decoded;

    BOOST_CHECK(decoded == original);
    BOOST_CHECK(readWriteManyStream.empty());

    CSizeComputer sizeComputer(SER_DISK, 0);
    sizeComputer << original;

    BOOST_CHECK_EQUAL(sizeComputer.size(), legacyStream.size());
    BOOST_CHECK_EQUAL(sizeComputer.GetType(), SER_DISK);
    BOOST_CHECK_EQUAL(sizeComputer.GetVersion(), 0);
}

BOOST_AUTO_TEST_CASE(varints)
{
    // Encode

    CDataStream ss(SER_DISK, 0);
    CDataStream::size_type size = 0;
    for (int i = 0; i < 100000; i++) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        BOOST_CHECK(size == ss.size());
    }

    for (uint64_t i = 0;  i < 100000000000ULL; i += 999999937) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        BOOST_CHECK(size == ss.size());
    }

    // decode
    for (int i = 0; i < 100000; i++) {
        int j = -1;
        ss >> VARINT(j);
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }

    for (uint64_t i = 0;  i < 100000000000ULL; i += 999999937) {
        uint64_t j = -1;
        ss >> VARINT(j);
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }
}

BOOST_AUTO_TEST_CASE(compactsize)
{
    CDataStream ss(SER_DISK, 0);
    std::vector<char>::size_type i, j;

    for (i = 1; i <= MAX_SIZE; i *= 2)
    {
        WriteCompactSize(ss, i-1);
        WriteCompactSize(ss, i);
    }
    for (i = 1; i <= MAX_SIZE; i *= 2)
    {
        j = ReadCompactSize(ss);
        BOOST_CHECK_MESSAGE((i-1) == j, "decoded:" << j << " expected:" << (i-1));
        j = ReadCompactSize(ss);
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }
}

static bool isCanonicalException(const std::ios_base::failure& ex)
{
    std::ios_base::failure expectedException("non-canonical ReadCompactSize()");

    // The string returned by what() can be different for different platforms.
    // Instead of directly comparing the ex.what() with an expected string,
    // create an instance of exception to see if ex.what() matches
    // the expected explanatory string returned by the exception instance.
    return strcmp(expectedException.what(), ex.what()) == 0;
}


BOOST_AUTO_TEST_CASE(noncanonical)
{
    // Write some non-canonical CompactSize encodings, and
    // make sure an exception is thrown when read back.
    CDataStream ss(SER_DISK, 0);
    std::vector<char>::size_type n;

    // zero encoded with three bytes:
    ss.write("\xfd\x00\x00", 3);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

    // 0xfc encoded with three bytes:
    ss.write("\xfd\xfc\x00", 3);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

    // 0xfd encoded with three bytes is OK:
    ss.write("\xfd\xfd\x00", 3);
    n = ReadCompactSize(ss);
    BOOST_CHECK(n == 0xfd);

    // zero encoded with five bytes:
    ss.write("\xfe\x00\x00\x00\x00", 5);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

    // 0xffff encoded with five bytes:
    ss.write("\xfe\xff\xff\x00\x00", 5);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

    // zero encoded with nine bytes:
    ss.write("\xff\x00\x00\x00\x00\x00\x00\x00\x00", 9);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

    // 0x01ffffff encoded with nine bytes:
    ss.write("\xff\xff\xff\xff\x01\x00\x00\x00\x00", 9);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);
}

BOOST_AUTO_TEST_CASE(insert_delete)
{
    // Test inserting/deleting bytes.
    CDataStream ss(SER_DISK, 0);
    BOOST_CHECK_EQUAL(ss.size(), 0);

    ss.write("\x00\x01\x02\xff", 4);
    BOOST_CHECK_EQUAL(ss.size(), 4);

    char c = (char)11;

    // Inserting at beginning/end/middle:
    ss.insert(ss.begin(), c);
    BOOST_CHECK_EQUAL(ss.size(), 5);
    BOOST_CHECK_EQUAL(ss[0], c);
    BOOST_CHECK_EQUAL(ss[1], 0);

    ss.insert(ss.end(), c);
    BOOST_CHECK_EQUAL(ss.size(), 6);
    BOOST_CHECK_EQUAL(ss[4], (char)0xff);
    BOOST_CHECK_EQUAL(ss[5], c);

    ss.insert(ss.begin()+2, c);
    BOOST_CHECK_EQUAL(ss.size(), 7);
    BOOST_CHECK_EQUAL(ss[2], c);

    // Delete at beginning/end/middle
    ss.erase(ss.begin());
    BOOST_CHECK_EQUAL(ss.size(), 6);
    BOOST_CHECK_EQUAL(ss[0], 0);

    ss.erase(ss.begin()+ss.size()-1);
    BOOST_CHECK_EQUAL(ss.size(), 5);
    BOOST_CHECK_EQUAL(ss[4], (char)0xff);

    ss.erase(ss.begin()+1);
    BOOST_CHECK_EQUAL(ss.size(), 4);
    BOOST_CHECK_EQUAL(ss[0], 0);
    BOOST_CHECK_EQUAL(ss[1], 1);
    BOOST_CHECK_EQUAL(ss[2], 2);
    BOOST_CHECK_EQUAL(ss[3], (char)0xff);

    // Make sure GetAndClear does the right thing:
    CSerializeData d;
    ss.GetAndClear(d);
    BOOST_CHECK_EQUAL(ss.size(), 0);
}

BOOST_AUTO_TEST_CASE(dip0002_transaction_serialization)
{
    {
        CMutableTransaction tx;
        tx.nVersion = 1;
        tx.nType = TRANSACTION_NORMAL;

        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << tx;

        const std::vector<unsigned char> expected = {
            0x01, 0x00, 0x00, 0x00,
            0x00,
            0x00,
            0x00, 0x00, 0x00, 0x00
        };

        const std::vector<unsigned char> serialized(
            stream.begin(),
            stream.end());

        BOOST_CHECK(serialized == expected);

        CMutableTransaction decoded;
        stream >> decoded;

        BOOST_CHECK_EQUAL(decoded.nVersion, 1);
        BOOST_CHECK_EQUAL(decoded.nType, TRANSACTION_NORMAL);
        BOOST_CHECK(decoded.vExtraPayload.empty());
        BOOST_CHECK(stream.empty());
    }

    {
        CMutableTransaction tx;
        tx.nVersion = CTransaction::SPECIAL_VERSION;
        tx.nType = TRANSACTION_NORMAL;

        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << tx;

        const std::vector<unsigned char> expected = {
            0x03, 0x00, 0x00, 0x00,
            0x00,
            0x00,
            0x00, 0x00, 0x00, 0x00
        };

        const std::vector<unsigned char> serialized(
            stream.begin(),
            stream.end());

        BOOST_CHECK(serialized == expected);

        CMutableTransaction decoded;
        stream >> decoded;

        BOOST_CHECK_EQUAL(
            decoded.nVersion,
            CTransaction::SPECIAL_VERSION);
        BOOST_CHECK_EQUAL(decoded.nType, TRANSACTION_NORMAL);
        BOOST_CHECK(decoded.vExtraPayload.empty());
        BOOST_CHECK(stream.empty());
    }

    {
        CMutableTransaction tx;
        tx.nVersion = CTransaction::SPECIAL_VERSION;
        tx.nType = 1;
        tx.vExtraPayload = {0x00};

        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << tx;

        const std::vector<unsigned char> expected = {
            0x03, 0x00, 0x01, 0x00,
            0x00,
            0x00,
            0x00, 0x00, 0x00, 0x00,
            0x01,
            0x00
        };

        const std::vector<unsigned char> serialized(
            stream.begin(),
            stream.end());

        BOOST_CHECK(serialized == expected);

        CMutableTransaction decoded;
        stream >> decoded;

        BOOST_CHECK_EQUAL(
            decoded.nVersion,
            CTransaction::SPECIAL_VERSION);
        BOOST_CHECK_EQUAL(decoded.nType, 1);
        BOOST_REQUIRE_EQUAL(decoded.vExtraPayload.size(), 1);
        BOOST_CHECK_EQUAL(decoded.vExtraPayload[0], 0x00);
        BOOST_CHECK(stream.empty());
    }

    {
        CMutableTransaction tx;
        tx.nVersion = CTransaction::SPECIAL_VERSION;
        tx.nType = TRANSACTION_NORMAL;
        tx.vExtraPayload = {0x01, 0x02, 0x03};

        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << tx;

        const std::vector<unsigned char> expected = {
            0x03, 0x00, 0x00, 0x00,
            0x00,
            0x00,
            0x00, 0x00, 0x00, 0x00
        };

        const std::vector<unsigned char> serialized(
            stream.begin(),
            stream.end());

        BOOST_CHECK(serialized == expected);

        CMutableTransaction decoded;
        decoded.vExtraPayload = {0xff};
        stream >> decoded;

        BOOST_CHECK_EQUAL(
            decoded.nVersion,
            CTransaction::SPECIAL_VERSION);
        BOOST_CHECK_EQUAL(decoded.nType, TRANSACTION_NORMAL);
        BOOST_CHECK(decoded.vExtraPayload.empty());
        BOOST_CHECK(stream.empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()