// Copyright (c) 2020-2022 The PIVX Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/pos_test_fixture.h"

#include "blocksignature.h"
#include "chainparams.h"
#include "kernel.h"
#include "main.h"
#include "node/miner.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(pos_validations_tests)

BOOST_FIXTURE_TEST_CASE(coinstake_tests, TestPoSChainSetup)
{
    const int lastPoWHeight = Params().GetConsensus().height_last_PoW;

    CBlockIndex* pindexPrev = nullptr;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(chainActive.Tip());
        BOOST_CHECK_EQUAL(chainActive.Height(), lastPoWHeight);
        pindexPrev = chainActive.Tip();
    }

    std::vector<CStakeableOutput> stakeableCoins;
    BOOST_REQUIRE(pwalletMain->StakeableCoins(&stakeableCoins));
    BOOST_REQUIRE(!stakeableCoins.empty());

    std::unique_ptr<CBlockTemplate> blockTemplate(
        CreateNewBlock(CScript(), pwalletMain, true));
    BOOST_REQUIRE(blockTemplate);

    CBlock block = blockTemplate->block;
    BOOST_REQUIRE(block.IsProofOfStake());
    BOOST_REQUIRE(block.vtx.size() >= 2);
    BOOST_CHECK(block.vtx[0].IsCoinBase());
    BOOST_CHECK(block.vtx[0].vout.size() == 1);
    BOOST_CHECK(block.vtx[0].vout[0].IsEmpty());
    BOOST_REQUIRE(block.vtx[1].IsCoinStake());
    BOOST_REQUIRE(block.vtx[1].vout.size() >= 2);
    BOOST_CHECK(block.vtx[1].vout[0].IsEmpty());

    bool hasRewardOutput = false;
    for (size_t i = 1; i < block.vtx[1].vout.size(); ++i) {
        if (!block.vtx[1].vout[i].IsEmpty() && block.vtx[1].vout[i].nValue > 0) {
            hasRewardOutput = true;
            break;
        }
    }
    BOOST_CHECK(hasRewardOutput);

    std::string stakeError;
    BOOST_CHECK(CheckProofOfStake(block, stakeError, pindexPrev));
    BOOST_CHECK_MESSAGE(stakeError.empty(), stakeError);
    BOOST_CHECK(CheckBlockSignature(block));

    CValidationState validityState;
    {
        LOCK(cs_main);
        BOOST_CHECK(TestBlockValidity(
            validityState,
            block,
            pindexPrev,
            false,
            true));
    }
    BOOST_CHECK(validityState.IsValid());

    const uint256 blockHash = block.GetHash();
    CValidationState processState;
    BOOST_REQUIRE(ProcessNewBlock(processState, nullptr, &block));
    BOOST_REQUIRE(processState.IsValid());

    {
        LOCK(cs_main);
        BOOST_REQUIRE(chainActive.Tip());
        BOOST_CHECK_EQUAL(chainActive.Height(), lastPoWHeight + 1);
        BOOST_CHECK(chainActive.Tip()->GetBlockHash() == blockHash);
    }
}

BOOST_AUTO_TEST_SUITE_END()