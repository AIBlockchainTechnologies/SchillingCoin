// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"
#include "script/interpreter.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/standard.h"
#include "test_schillingcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(script_P2CS_tests, BasicTestingSetup)

static void CheckValidKeyId(const CTxDestination& dest, const CKeyID& expectedKey)
{
    const CKeyID* keyid = boost::get<CKeyID>(&dest);
    if (keyid) {
        BOOST_CHECK(*keyid == expectedKey);
    } else {
        BOOST_ERROR("Destination is not a CKeyID");
    }
}

BOOST_AUTO_TEST_CASE(extract_cold_staking_destination_keys)
{
    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    CKeyID ownerId = ownerKey.GetPubKey().GetID();

    CKey stakerKey;
    stakerKey.MakeNewKey(true);
    CKeyID stakerId = stakerKey.GetPubKey().GetID();

    CScript script = GetScriptForStakeDelegation(stakerId, ownerId);

    CTxDestination ownerDest;
    BOOST_CHECK(ExtractDestination(script, ownerDest, false));
    CheckValidKeyId(ownerDest, ownerId);

    CTxDestination stakerDest;
    BOOST_CHECK(ExtractDestination(script, stakerDest, true));
    CheckValidKeyId(stakerDest, stakerId);

    txnouttype type;
    int nRequiredRet = -1;
    std::vector<CTxDestination> destVector;

    BOOST_CHECK(ExtractDestinations(script, type, destVector, nRequiredRet));
    BOOST_CHECK(type == TX_COLDSTAKE);
    BOOST_CHECK(nRequiredRet == 2);
    BOOST_CHECK(destVector.size() == 2);
    CheckValidKeyId(destVector[0], stakerId);
    CheckValidKeyId(destVector[1], ownerId);
}

BOOST_AUTO_TEST_CASE(fake_script_recognition)
{
    CKey ownerKey;
    ownerKey.MakeNewKey(true);

    CKey stakerKey;
    stakerKey.MakeNewKey(true);

    CScript fakeScript;
    fakeScript << opcodetype(0x2F) << opcodetype(0x01) << OP_ROT
               << OP_IF << OP_CHECKCOLDSTAKEVERIFY << ToByteVector(stakerKey.GetPubKey().GetID())
               << OP_ELSE << ToByteVector(ownerKey.GetPubKey().GetID()) << OP_DROP
               << OP_EQUALVERIFY << OP_CHECKSIG;

    BOOST_CHECK_MESSAGE(!fakeScript.IsPayToColdStaking(),
                        "Fake script passes as P2CS");
}

BOOST_AUTO_TEST_SUITE_END()