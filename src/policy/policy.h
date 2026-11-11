// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_POLICY_POLICY_H
#define SCHILLINGCOIN_POLICY_POLICY_H

#include "consensus/consensus.h"
#include "feerate.h"
#include "script/interpreter.h"
#include "script/standard.h"

#include <string>

class CCoinsViewCache;
class CTxOut;

/** Default for -blockmaxsize and -blockminsize, which control the range of sizes the mining code will create. */
static constexpr unsigned int DEFAULT_BLOCK_MAX_SIZE{750000};
static constexpr unsigned int DEFAULT_BLOCK_MIN_SIZE{0};

/** Default for -blockprioritysize, maximum space for zero/low-fee transactions. */
static constexpr unsigned int DEFAULT_BLOCK_PRIORITY_SIZE{50000};

/** Default for -blockmintxfee, which sets the minimum feerate for transactions created by mining code. */
static constexpr unsigned int DEFAULT_BLOCK_MIN_TX_FEE{1000};

/** The maximum serialized size for transactions SCH is willing to relay or mine. */
static constexpr unsigned int MAX_STANDARD_TX_SIZE{100000};

/** Maximum number of signature check operations in an IsStandard() P2SH script. */
static constexpr unsigned int MAX_P2SH_SIGOPS{15};

/** Default for -incrementalrelayfee. */
static constexpr unsigned int DEFAULT_INCREMENTAL_RELAY_FEE{1000};

/** Default for -bytespersigop. */
static constexpr unsigned int DEFAULT_BYTES_PER_SIGOP{20};

/** Default for -minrelaytxfee. */
static constexpr unsigned int DEFAULT_MIN_RELAY_TX_FEE{1000};

/** Maximum number of transactions per cluster. */
static constexpr unsigned int DEFAULT_CLUSTER_LIMIT{64};

/** Maximum size of a cluster in virtual kilobytes. */
static constexpr unsigned int DEFAULT_CLUSTER_SIZE_LIMIT_KVB{101};

/** Default for -limitancestorcount. */
static constexpr unsigned int DEFAULT_ANCESTOR_LIMIT{25};

/** Default for -limitancestorsize, in kilobytes. */
static constexpr unsigned int DEFAULT_ANCESTOR_SIZE_LIMIT_KVB{101};

/** Default for -limitdescendantcount. */
static constexpr unsigned int DEFAULT_DESCENDANT_LIMIT{25};

/** Default for -limitdescendantsize, in kilobytes. */
static constexpr unsigned int DEFAULT_DESCENDANT_SIZE_LIMIT_KVB{101};

/** Default for accepting data-carrier transactions. */
static constexpr bool DEFAULT_ACCEPT_DATACARRIER{true};

/** Default for accepting bare multisig transactions. */
static constexpr bool DEFAULT_PERMIT_BAREMULTISIG{true};
extern bool fIsBareMultisigStd;

/**
 * Minimum fee rate used for defining dust. This is intentionally
 * independent from the configurable relay fee.
 */
static constexpr unsigned int DUST_RELAY_TX_FEE{30000};

/**
 * Standard script verification flags that standard transactions will comply
 * with. However scripts violating these flags may still be present in valid
 * blocks and we must accept those blocks.
 */
static constexpr unsigned int STANDARD_SCRIPT_VERIFY_FLAGS = MANDATORY_SCRIPT_VERIFY_FLAGS |
                                                              SCRIPT_VERIFY_DERSIG |
                                                              SCRIPT_VERIFY_STRICTENC |
                                                              SCRIPT_VERIFY_MINIMALDATA |
                                                              SCRIPT_VERIFY_NULLDUMMY |
                                                              SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS;

/** For convenience, standard but not mandatory verify flags. */
static constexpr unsigned int STANDARD_NOT_MANDATORY_VERIFY_FLAGS = STANDARD_SCRIPT_VERIFY_FLAGS & ~MANDATORY_SCRIPT_VERIFY_FLAGS;

static_assert(DEFAULT_BLOCK_MAX_SIZE <= MAX_BLOCK_SIZE_CURRENT, "DEFAULT_BLOCK_MAX_SIZE must not exceed MAX_BLOCK_SIZE_CURRENT");
static_assert(DEFAULT_BLOCK_PRIORITY_SIZE <= DEFAULT_BLOCK_MAX_SIZE, "DEFAULT_BLOCK_PRIORITY_SIZE must not exceed DEFAULT_BLOCK_MAX_SIZE");

CAmount GetDustThreshold(const CTxOut& txout, const CFeeRate& dustRelayFeeIn);
CAmount GetDustThreshold(const CFeeRate& dustRelayFeeIn);
bool IsDust(const CTxOut& txout, const CFeeRate& dustRelayFeeIn);

bool IsStandard(const CScript& scriptPubKey, TxoutType& whichType);
bool IsStandardTx(const CTransaction& tx, std::string& reason);
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs);

/** Compute the virtual transaction size (weight reinterpreted as bytes). */
int64_t GetVirtualTransactionSize(int64_t nWeight, int64_t nSigOpCost, unsigned int bytes_per_sigop);
int64_t GetVirtualTransactionSize(const CTransaction& tx, int64_t nSigOpCost, unsigned int bytes_per_sigop);

static inline int64_t GetVirtualTransactionSize(const CTransaction& tx)
{
    return GetVirtualTransactionSize(tx, 0, 0);
}

int64_t GetSigOpsAdjustedWeight(int64_t weight, int64_t sigop_cost, unsigned int bytes_per_sigop);

extern CFeeRate dustRelayFee;

#endif // SCHILLINGCOIN_POLICY_POLICY_H