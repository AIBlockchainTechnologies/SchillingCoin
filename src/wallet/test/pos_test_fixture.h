// Copyright (c) 2021 The PIVX Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_TEST_POS_TEST_FIXTURE_H
#define SCHILLINGCOIN_WALLET_TEST_POS_TEST_FIXTURE_H

#include "test/test_schillingcoin.h"

#include "key.h"
#include "primitives/block.h"

#include <memory>
#include <vector>

class CBlockTemplate;

/**
 * Test fixture with a regtest chain advanced through SCH's proof-of-work
 * phase and a synchronized wallet containing the block-generation key.
 *
 * TestingSetup owns the global test wallet and its validation-interface
 * registration. This fixture uses that wallet without replacing, deleting,
 * or registering it a second time.
 */
struct TestPoSChainSetup : public TestingSetup
{
    CKey coinbaseKey;
    std::vector<CTransaction> coinbaseTxns;

    TestPoSChainSetup();
    ~TestPoSChainSetup();

private:
    CBlock CreateAndProcessBlock();
};

#endif // SCHILLINGCOIN_WALLET_TEST_POS_TEST_FIXTURE_H
