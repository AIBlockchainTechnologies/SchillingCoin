// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"
#include "main.h"
#include "test_schillingcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(block_reward_schedule_test)
{
    const Consensus::Params& consensus = Params().GetConsensus();

    const CAmount initialReward = 22000000 * COIN;

    // Preserve SCH's historical consensus values produced by the
    // floating-point reward literals in GetBlockValue().
    const CAmount standardReward = 3229999999;
    const CAmount reducedReward = 1614999999;

    // Height zero uses schedule height zero.
    BOOST_CHECK_EQUAL(GetBlockValue(0), initialReward);

    // Before fee-reward activation, SCH preserves its historical
    // previous-height reward lookup. Height one therefore also evaluates
    // schedule height zero.
    BOOST_CHECK_EQUAL(GetBlockValue(1), initialReward);

    // Subsequent historical heights use the standard reward.
    BOOST_CHECK_EQUAL(GetBlockValue(2), standardReward);
    BOOST_CHECK_EQUAL(
        GetBlockValue(consensus.FeeRewardDistributionHeight - 1),
        standardReward);

    // Fee-reward activation removes the historical one-height offset.
    // The subsidy itself remains at the standard rate at this boundary.
    BOOST_CHECK_EQUAL(
        GetBlockValue(consensus.FeeRewardDistributionHeight),
        standardReward);
    BOOST_CHECK_EQUAL(
        GetBlockValue(consensus.FeeRewardDistributionHeight + 1),
        standardReward);

    // Verify the reward-reduction boundary.
    BOOST_CHECK_EQUAL(GetBlockValue(5783306), standardReward);
    BOOST_CHECK_EQUAL(GetBlockValue(5783307), reducedReward);
    BOOST_CHECK_EQUAL(GetBlockValue(5783308), reducedReward);

    // Every tested reward must remain within the active network's
    // consensus money range.
    BOOST_CHECK(consensus.MoneyRange(initialReward));
    BOOST_CHECK(consensus.MoneyRange(standardReward));
    BOOST_CHECK(consensus.MoneyRange(reducedReward));
}

BOOST_AUTO_TEST_SUITE_END()