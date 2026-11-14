// Copyright (c) 2012-2022 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dbwrapper.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/string.h>

#include <memory>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(dbwrapper_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dbwrapper)
{
    fs::path ph = m_args.GetDataDirBase() / "dbwrapper";
    CDBWrapper dbw(ph, (1 << 20), true, false);
    uint8_t key{'k'};
    uint256 in = InsecureRand256();
    uint256 res;

    BOOST_CHECK(dbw.Write(key, in));
    BOOST_CHECK(dbw.Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
}

BOOST_AUTO_TEST_CASE(dbwrapper_basic_data)
{
    fs::path ph = m_args.GetDataDirBase() / "dbwrapper_basic_data";
    CDBWrapper dbw(ph, (1 << 20), false, true);

    uint256 res;
    uint32_t res_uint_32;
    bool res_bool;

        //Simulate block raw data - "b + block hash"
        std::string key_block = "b" + InsecureRand256().ToString();

        uint256 in_block = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_block, in_block));
        BOOST_CHECK(dbw.Read(key_block, res));
        BOOST_CHECK_EQUAL(res.ToString(), in_block.ToString());

        //Simulate file raw data - "f + file_number"
        std::string key_file = strprintf("f%04x", InsecureRand32());

        uint256 in_file_info = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_file, in_file_info));
        BOOST_CHECK(dbw.Read(key_file, res));
        BOOST_CHECK_EQUAL(res.ToString(), in_file_info.ToString());

        //Simulate transaction raw data - "t + transaction hash"
        std::string key_transaction = "t" + InsecureRand256().ToString();

        uint256 in_transaction = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_transaction, in_transaction));
        BOOST_CHECK(dbw.Read(key_transaction, res));
        BOOST_CHECK_EQUAL(res.ToString(), in_transaction.ToString());

        //Simulate UTXO raw data - "c + transaction hash"
        std::string key_utxo = "c" + InsecureRand256().ToString();

        uint256 in_utxo = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_utxo, in_utxo));
        BOOST_CHECK(dbw.Read(key_utxo, res));
        BOOST_CHECK_EQUAL(res.ToString(), in_utxo.ToString());

        //Simulate last block file number - "l"
        uint8_t key_last_blockfile_number{'l'};
        uint32_t lastblockfilenumber = InsecureRand32();
        BOOST_CHECK(dbw.Write(key_last_blockfile_number, lastblockfilenumber));
        BOOST_CHECK(dbw.Read(key_last_blockfile_number, res_uint_32));
        BOOST_CHECK_EQUAL(lastblockfilenumber, res_uint_32);

        //Simulate Is Reindexing - "R"
        uint8_t key_IsReindexing{'R'};
        bool isInReindexing = InsecureRandBool();
        BOOST_CHECK(dbw.Write(key_IsReindexing, isInReindexing));
        BOOST_CHECK(dbw.Read(key_IsReindexing, res_bool));
        BOOST_CHECK_EQUAL(isInReindexing, res_bool);

        //Simulate last block hash up to which UXTO covers - 'B'
        uint8_t key_lastblockhash_uxto{'B'};
        uint256 lastblock_hash = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_lastblockhash_uxto, lastblock_hash));
        BOOST_CHECK(dbw.Read(key_lastblockhash_uxto, res));
        BOOST_CHECK_EQUAL(lastblock_hash, res);

        //Simulate file raw data - "F + filename_number + filename"
        std::string file_option_tag = "F";
        uint8_t filename_length = InsecureRandBits(8);
        std::string filename = "randomfilename";
        std::string key_file_option = strprintf("%s%01x%s", file_option_tag,filename_length,filename);

        bool in_file_bool = InsecureRandBool();
    BOOST_CHECK(dbw.Write(key_file_option, in_file_bool));
    BOOST_CHECK(dbw.Read(key_file_option, res_bool));
    BOOST_CHECK_EQUAL(res_bool, in_file_bool);
}

// Test batch operations
BOOST_AUTO_TEST_CASE(dbwrapper_batch)
{
    fs::path ph = m_args.GetDataDirBase() / "dbwrapper_batch";
    CDBWrapper dbw(ph, (1 << 20), true, false);

    uint8_t key{'i'};
    uint256 in = InsecureRand256();
    uint8_t key2{'j'};
    uint256 in2 = InsecureRand256();
    uint8_t key3{'k'};
    uint256 in3 = InsecureRand256();

    uint256 res;
    CDBBatch batch(dbw);

    batch.Write(key, in);
    batch.Write(key2, in2);
    batch.Write(key3, in3);

    // Remove key3 before it is written.
    batch.Erase(key3);

    BOOST_CHECK(dbw.WriteBatch(batch));

    BOOST_CHECK(dbw.Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
    BOOST_CHECK(dbw.Read(key2, res));
    BOOST_CHECK_EQUAL(res.ToString(), in2.ToString());

    // key3 should never have been written.
    BOOST_CHECK(!dbw.Read(key3, res));
}

BOOST_AUTO_TEST_CASE(dbwrapper_iterator)
{
    fs::path ph = m_args.GetDataDirBase() / "dbwrapper_iterator";
    CDBWrapper dbw(ph, (1 << 20), true, false);

    // The two keys are intentionally chosen for ordering.
    uint8_t key{'j'};
    uint256 in = InsecureRand256();
    BOOST_CHECK(dbw.Write(key, in));

    uint8_t key2{'k'};
    uint256 in2 = InsecureRand256();
    BOOST_CHECK(dbw.Write(key2, in2));

    std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());
    it->Seek(key);

    uint8_t key_res;
    uint256 val_res;

    BOOST_REQUIRE(it->GetKey(key_res));
    BOOST_REQUIRE(it->GetValue(val_res));
    BOOST_CHECK_EQUAL(key_res, key);
    BOOST_CHECK_EQUAL(val_res.ToString(), in.ToString());

    it->Next();

    BOOST_REQUIRE(it->GetKey(key_res));
    BOOST_REQUIRE(it->GetValue(val_res));
    BOOST_CHECK_EQUAL(key_res, key2);
    BOOST_CHECK_EQUAL(val_res.ToString(), in2.ToString());

    it->Next();
    BOOST_CHECK(!it->Valid());
}

BOOST_AUTO_TEST_CASE(iterator_ordering)
{
    fs::path ph = m_args.GetDataDirBase() / "iterator_ordering";
    CDBWrapper dbw(ph, (1 << 20), true, false);
    for (int x=0x00; x<256; ++x) {
        uint8_t key = x;
        uint32_t value = x*x;
        if (!(x & 1)) BOOST_CHECK(dbw.Write(key, value));
    }

    // Check that creating an iterator creates a snapshot
    std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());

    for (unsigned int x=0x00; x<256; ++x) {
        uint8_t key = x;
        uint32_t value = x*x;
        if (x & 1) BOOST_CHECK(dbw.Write(key, value));
    }

    for (const int seek_start : {0x00, 0x80}) {
        it->Seek((uint8_t)seek_start);
        for (unsigned int x=seek_start; x<255; ++x) {
            uint8_t key;
            uint32_t value;
            BOOST_CHECK(it->Valid());
            if (!it->Valid()) // Avoid spurious errors about invalid iterator's key and value in case of failure
                break;
            BOOST_CHECK(it->GetKey(key));
            if (x & 1) {
                BOOST_CHECK_EQUAL(key, x + 1);
                continue;
            }
            BOOST_CHECK(it->GetValue(value));
            BOOST_CHECK_EQUAL(key, x);
            BOOST_CHECK_EQUAL(value, x*x);
            it->Next();
        }
        BOOST_CHECK(!it->Valid());
    }
}

struct StringContentsSerializer {
    // Used to make two serialized objects the same while letting them have different lengths
    // This is a terrible idea
    std::string str;
    StringContentsSerializer() = default;
    explicit StringContentsSerializer(const std::string& inp) : str(inp) {}

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        for (size_t i = 0; i < str.size(); i++) {
            s << uint8_t(str[i]);
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        str.clear();
        uint8_t c{0};
        while (!s.eof()) {
            s >> c;
            str.push_back(c);
        }
    }
};

BOOST_AUTO_TEST_CASE(iterator_string_ordering)
{
    fs::path ph = m_args.GetDataDirBase() / "iterator_string_ordering";
    CDBWrapper dbw(ph, (1 << 20), true, false);
    for (int x = 0; x < 10; ++x) {
        for (int y = 0; y < 10; ++y) {
            std::string key{ToString(x)};
            for (int z = 0; z < y; ++z)
                key += key;
            uint32_t value = x*x;
            BOOST_CHECK(dbw.Write(StringContentsSerializer{key}, value));
        }
    }

    std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());
    for (const int seek_start : {0, 5}) {
        it->Seek(StringContentsSerializer{ToString(seek_start)});
        for (unsigned int x = seek_start; x < 10; ++x) {
            for (int y = 0; y < 10; ++y) {
                std::string exp_key{ToString(x)};
                for (int z = 0; z < y; ++z)
                    exp_key += exp_key;
                StringContentsSerializer key;
                uint32_t value;
                BOOST_CHECK(it->Valid());
                if (!it->Valid()) // Avoid spurious errors about invalid iterator's key and value in case of failure
                    break;
                BOOST_CHECK(it->GetKey(key));
                BOOST_CHECK(it->GetValue(value));
                BOOST_CHECK_EQUAL(key.str, exp_key);
                BOOST_CHECK_EQUAL(value, x*x);
                it->Next();
            }
        }
        BOOST_CHECK(!it->Valid());
    }
}

BOOST_AUTO_TEST_CASE(unicodepath)
{
    // Attempt to create a database with a UTF8 character in the path.
    // On Windows this test will fail if the directory is created using
    // the ANSI CreateDirectoryA call and the code page isn't UTF8.
    // It will succeed if created with CreateDirectoryW.
    fs::path ph = m_args.GetDataDirBase() / "test_runner_∋_🏃_20191128_104644";
    CDBWrapper dbw(ph, (1 << 20));

    fs::path lockPath = ph / "LOCK";
    BOOST_CHECK(fs::exists(lockPath));
}

BOOST_AUTO_TEST_SUITE_END()