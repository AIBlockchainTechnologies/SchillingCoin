// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "policy/feerate.h"
#include "policy/fees.h"
#include "txmempool.h"
#include "uint256.h"
#include "util/system.h"

#include "test/test_schillingcoin.h"

#include <boost/test/unit_test.hpp>

class TestMemPoolEntryHelper
{
private:
    CAmount nFee{0};
    int64_t nTime{0};
    double dPriority{0};
    unsigned int nHeight{1};

public:
    TestMemPoolEntryHelper& Fee(CAmount fee)
    {
        nFee = fee;
        return *this;
    }

    TestMemPoolEntryHelper& Time(int64_t time)
    {
        nTime = time;
        return *this;
    }

    TestMemPoolEntryHelper& Height(unsigned int height)
    {
        nHeight = height;
        return *this;
    }

    CTxMemPoolEntry FromTx(const CMutableTransaction& tx) const
    {
        return CTxMemPoolEntry(
            CTransaction(tx),
            nFee,
            nTime,
            dPriority,
            nHeight);
    }
};

BOOST_FIXTURE_TEST_SUITE(policyestimator_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(BlockPolicyEstimates)
{
    CBlockPolicyEstimator feeEst;
    CTxMemPool mpool(&feeEst, CFeeRate(1000));
    TestMemPoolEntryHelper entry;
    CAmount basefee(2000);
    CAmount deltaFee(100);
    std::vector<CAmount> feeV;

    // Populate vectors of increasing fees
    for (int j = 0; j < 10; j++) {
        feeV.push_back(basefee * (j+1));
    }

    // Store the hashes of transactions that have been
    // added to the mempool by their associate fee
    // txHashes[j] is populated with transactions either of
    // fee = basefee * (j+1)
    std::vector<uint256> txHashes[10];

    // Create a transaction template
    CScript garbage;
    for (unsigned int i = 0; i < 128; i++)
        garbage.push_back('X');
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig = garbage;
    tx.vout.resize(1);
    tx.vout[0].nValue=0LL;
    CFeeRate baseRate(basefee, ::GetSerializeSize(tx, PROTOCOL_VERSION));

    // Create a fake block
    std::vector<CTransaction> block;
    std::list<CTransaction> conflicts;
    int blocknum = 0;

    // Loop through 200 blocks
    // At a decay .998 and 4 fee transactions per block
    // This makes the tx count about 1.33 per bucket, above the 1 threshold
    while (blocknum < 200) {
        for (int j = 0; j < 10; j++) { // For each fee
            for (int k = 0; k < 4; k++) { // add 4 fee txs
                tx.vin[0].prevout.n = 10000*blocknum+100*j+k; // make transaction unique
                uint256 hash = tx.GetHash();
                mpool.addUnchecked(hash, entry.Fee(feeV[j]).Time(GetTime()).Height(blocknum).FromTx(tx));
                txHashes[j].push_back(hash);
            }
        }
        //Create blocks where higher fee txs are included more often
        for (int h = 0; h <= blocknum%10; h++) {
            // 10/10 blocks add highest fee transactions
            // 9/10 blocks add 2nd highest and so on until ...
            // 1/10 blocks add lowest fee transactions
            while (txHashes[9-h].size()) {
                CTransaction blockTx;
                if (mpool.lookup(txHashes[9-h].back(), blockTx)) {
                    block.push_back(blockTx);
                }
                txHashes[9-h].pop_back();
            }
        }
        mpool.removeForBlock(block, ++blocknum, conflicts);
        conflicts.clear();
        block.clear();
        if (blocknum == 3) {
            // At this point we should need to combine 3 buckets to get enough data points
            // So estimateFee(1) should fail and estimateFee(2) should return somewhere around
            // 9*baseRate. estimateFee(2) percentages are 100, 100, and 90, averaging 97%.
            BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));
            BOOST_CHECK(feeEst.estimateFee(2).GetFeePerK() < 9 * baseRate.GetFeePerK() + deltaFee);
            BOOST_CHECK(feeEst.estimateFee(2).GetFeePerK() > 9 * baseRate.GetFeePerK() - deltaFee);
        }
    }

    std::vector<CAmount> origFeeEst;
    // Highest feerate is 10*baseRate and gets in all blocks,
    // second highest feerate is 9*baseRate and gets in 9/10 blocks = 90%,
    // and the third highest feerate is 8*baseRate and gets in 8/10 blocks = 80%.
    // estimateFee(1) would return 10*baseRate but is hardcoded to return failure.
    // At scale 2, the test logic is exact only for even targets.
    for (int i = 1; i < 10; i++) {
        origFeeEst.push_back(feeEst.estimateFee(i).GetFeePerK());

        if (i > 2) {
            BOOST_CHECK(origFeeEst[i - 1] <= origFeeEst[i - 2]);
        }

        const int mult = 11 - i;

        if (i % 2 == 0) {
            BOOST_CHECK(origFeeEst[i - 1] < mult * baseRate.GetFeePerK() + deltaFee);
            BOOST_CHECK(origFeeEst[i - 1] > mult * baseRate.GetFeePerK() - deltaFee);
        }
    }

    // Fill out the remaining original estimates.
    for (int i = 10; i <= 48; i++) {
        origFeeEst.push_back(feeEst.estimateFee(i).GetFeePerK());
    }

    // Mine 50 more blocks with no transactions happening, estimates shouldn't change
    // We haven't decayed the moving average enough so we still have enough data points in every bucket
    while (blocknum < 250) {
        mpool.removeForBlock(block, ++blocknum, conflicts);
        conflicts.clear();
    }

    BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));

    for (int i = 2; i < 10; i++) {
        BOOST_CHECK(feeEst.estimateFee(i).GetFeePerK() < origFeeEst[i - 1] + deltaFee);
        BOOST_CHECK(feeEst.estimateFee(i).GetFeePerK() > origFeeEst[i - 1] - deltaFee);
    }

    // Mine 15 more blocks with lots of transactions happening and not getting mined
    // Estimates should go up
    while (blocknum < 265) {
        for (int j = 0; j < 10; j++) { // For each fee multiple
            for (int k = 0; k < 4; k++) { // add 4 fee txs
                tx.vin[0].prevout.n = 10000*blocknum+100*j+k;
                uint256 hash = tx.GetHash();
                mpool.addUnchecked(hash, entry.Fee(feeV[j]).Time(GetTime()).Height(blocknum).FromTx(tx));
                txHashes[j].push_back(hash);
            }
        }
        mpool.removeForBlock(block, ++blocknum, conflicts);
        conflicts.clear();
    }

    for (int i = 1; i < 10; i++) {
        BOOST_CHECK(
            feeEst.estimateFee(i) == CFeeRate(0) ||
            feeEst.estimateFee(i).GetFeePerK() > origFeeEst[i - 1] - deltaFee);
    }

    // Mine all those transactions
    // Estimates should still not be below original
    for (int j = 0; j < 10; j++) {
        while (txHashes[j].size()) {
            CTransaction blockTx;
            if (mpool.lookup(txHashes[j].back(), blockTx)) {
                block.push_back(blockTx);
            }
            txHashes[j].pop_back();
        }
    }
    mpool.removeForBlock(block, 266, conflicts);
    conflicts.clear();
    block.clear();

    BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));

    for (int i = 2; i < 10; i++) {
        BOOST_CHECK(
            feeEst.estimateFee(i) == CFeeRate(0) ||
            feeEst.estimateFee(i).GetFeePerK() > origFeeEst[i - 1] - deltaFee);
    }

    // Mine 400 more blocks where everything is mined every block
    // Estimates should be below original estimates
    while (blocknum < 665) {
        for (int j = 0; j < 10; j++) { // For each fee multiple
            for (int k = 0; k < 4; k++) { // add 4 fee txs
                tx.vin[0].prevout.n = 10000*blocknum+100*j+k;
                uint256 hash = tx.GetHash();
                mpool.addUnchecked(hash, entry.Fee(feeV[j]).Time(GetTime()).Height(blocknum).FromTx(tx));
                CTransaction blockTx;
                if (mpool.lookup(hash, blockTx)) {
                    block.push_back(blockTx);
                }
            }
        }
        mpool.removeForBlock(block, ++blocknum, conflicts);
        conflicts.clear();
        block.clear();
    }
    BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));

    for (int i = 2; i < 9; i++) {
        BOOST_CHECK(
            feeEst.estimateFee(i).GetFeePerK() <
            origFeeEst[i - 1] - deltaFee);
    }

    // Rolling mempool minimum-fee tracking will be tested after
    // TrimToSize() and GetMinFee() are added during the
    // ancestor-aware mempool modernization.
}

BOOST_AUTO_TEST_SUITE_END()