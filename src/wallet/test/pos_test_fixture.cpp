// Copyright (c) 2021 The PIVX Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/pos_test_fixture.h"

#include "arith_uint256.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "main.h"
#include "node/miner.h"
#include "pow.h"
#include "script/standard.h"
#include "util.h"
#include "validationinterface.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <memory>

CBlock TestPoSChainSetup::CreateAndProcessBlock()
{
    const CScript scriptPubKey = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
    std::unique_ptr<CBlockTemplate> blockTemplate(
        CreateNewBlock(scriptPubKey, pwalletMain, false));
    BOOST_REQUIRE(blockTemplate);

    CBlock block = blockTemplate->block;
    CBlockIndex* pindexPrev = nullptr;
    {
        LOCK(cs_main);
        pindexPrev = chainActive.Tip();
    }
    BOOST_REQUIRE(pindexPrev);

    unsigned int extraNonce = 0;
    IncrementExtraNonce(&block, pindexPrev, extraNonce);

    while (!CheckProofOfWork(block.GetHash(), block.nBits)) {
        ++block.nNonce;
        if (block.nNonce == 0) {
            ++block.nTime;
        }
    }

    CValidationState state;
    BOOST_REQUIRE(ProcessNewBlock(state, nullptr, &block));
    BOOST_REQUIRE(state.IsValid());
    return block;
}

TestPoSChainSetup::TestPoSChainSetup() : TestingSetup(CBaseChainParams::REGTEST)
{
    coinbaseKey.MakeNewKey(true);
    BOOST_REQUIRE(coinbaseKey.IsValid());

    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_REQUIRE(pwalletMain->AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey()));
    }

    const int lastPoWHeight = Params().GetConsensus().height_last_PoW;
    while (true) {
        int currentHeight;
        {
            LOCK(cs_main);
            currentHeight = chainActive.Height();
        }
        if (currentHeight >= lastPoWHeight) {
            break;
        }

        CBlock block = CreateAndProcessBlock();
        coinbaseTxns.emplace_back(block.vtx[0]);
    }

    int finalHeight;
    {
        LOCK(cs_main);
        finalHeight = chainActive.Height();
    }
    BOOST_REQUIRE_EQUAL(finalHeight, lastPoWHeight);

    std::vector<CStakeableOutput> stakeableCoins;
    BOOST_REQUIRE(pwalletMain->StakeableCoins(&stakeableCoins));
    BOOST_REQUIRE(!stakeableCoins.empty());
}

TestPoSChainSetup::~TestPoSChainSetup()
{
    SetMockTime(0);
}