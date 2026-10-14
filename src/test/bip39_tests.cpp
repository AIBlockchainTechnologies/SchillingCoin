// Copyright (c) 2014-2020 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "data/bip39_vectors.json.h"
#include "key.h"
#include "test/test_schillingcoin.h"
#include "utilstrencodings.h"
#include "wallet/bip39.h"

#include <boost/test/unit_test.hpp>

#include <string>
#include <univalue.h>
#include <vector>

// In script_tests.cpp
extern UniValue read_json(const std::string& jsondata);

static SecureString JoinMnemonicWords(const std::vector<std::string>& words)
{
    std::string mnemonic;

    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0) {
            mnemonic += ' ';
        }

        mnemonic += words[i];
    }

    return SecureString(mnemonic.begin(), mnemonic.end());
}

BOOST_FIXTURE_TEST_SUITE(bip39_tests, BasicTestingSetup)

// Official Trezor BIP39 vectors.
BOOST_AUTO_TEST_CASE(bip39_vectors)
{
    UniValue tests = read_json(std::string(
        json_tests::bip39_vectors,
        json_tests::bip39_vectors + sizeof(json_tests::bip39_vectors)));

    for (unsigned int i = 0; i < tests.size(); ++i) {
        const UniValue test = tests[i];
        const std::string strTest = test.write();

        if (test.size() < 4) {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }

        const std::vector<uint8_t> entropyBytes =
            ParseHex(test[0].get_str());

        const SecureVector entropy(
            entropyBytes.begin(),
            entropyBytes.end());

        const std::vector<std::string> generatedWords =
            CMnemonic::FromData(entropy, entropy.size());

        const SecureString generatedMnemonic =
            JoinMnemonicWords(generatedWords);

        const std::string expectedMnemonicString =
            test[1].get_str();

        const SecureString expectedMnemonic(
            expectedMnemonicString.begin(),
            expectedMnemonicString.end());

        BOOST_CHECK(generatedMnemonic == expectedMnemonic);
        BOOST_CHECK(CMnemonic::Check(generatedMnemonic));
        BOOST_CHECK(CMnemonic::Check(expectedMnemonic));

        SecureVector seed;
        const SecureString passphrase("TREZOR");

        CMnemonic::ToSeed(
            expectedMnemonic,
            passphrase,
            seed);

        BOOST_REQUIRE_EQUAL(seed.size(), 64U);
        BOOST_CHECK(HexStr(seed) == test[2].get_str());

        CExtKey masterKey;
        masterKey.SetMaster(&seed[0], seed.size());

        unsigned char encodedMaster[BIP32_EXTKEY_SIZE];
        masterKey.Encode(encodedMaster);

        // The official vectors use Bitcoin's xprv prefix. SCH intentionally
        // uses its own extended-key prefix, so compare the underlying BIP32
        // payload independently of the four network-specific prefix bytes.
        std::vector<unsigned char> expectedExtendedKey;

        BOOST_REQUIRE(
            DecodeBase58Check(
                test[3].get_str(),
                expectedExtendedKey));

        BOOST_REQUIRE_EQUAL(
            expectedExtendedKey.size(),
            BIP32_EXTKEY_SIZE + 4);

        const std::vector<unsigned char> expectedPayload(
            expectedExtendedKey.begin() + 4,
            expectedExtendedKey.end());

        const std::vector<unsigned char> actualPayload(
            encodedMaster,
            encodedMaster + BIP32_EXTKEY_SIZE);

        BOOST_CHECK(expectedPayload == actualPayload);

        // Verify SCH extended private-key Base58 encoding and decoding.
        CBitcoinExtKey schExtendedKey;
        schExtendedKey.SetKey(masterKey);

        const std::string schEncodedKey =
            schExtendedKey.ToString();

        CBitcoinExtKey schDecodedExtendedKey(
            schEncodedKey);

        const CExtKey decodedMasterKey =
            schDecodedExtendedKey.GetKey();

        BOOST_CHECK(decodedMasterKey == masterKey);
    }
}

BOOST_AUTO_TEST_SUITE_END()