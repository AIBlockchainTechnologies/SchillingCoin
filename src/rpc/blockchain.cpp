// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainlock/chainlock.h"
#include "chainparams.h"
#include "clientversion.h"
#include "core_io.h"
#include "deploymentinfo.h"
#include "deploymentstatus.h"
#include "evo/cbtx.h"
#include "evo/chainhelper.h"
#include "evo/evodb.h"
#include "evo/mnhftx.h"
#include "evo/specialtx.h"
#include "hash.h"
#include "index/coinstatsindex.h"
#include "init.h"
#include "instantsend/instantsend.h"
#include "kernel.h"
#include "kernel/coinstats.h"
#include "key_io.h"
#include "node/blockstorage.h"
#include "node/context.h"
#include "policy/feerate.h"
#include "rpc/blockchain.h"
#include "rpc/server.h"
#include "rpc/server_util.h"
#include "rpc/util.h"
#include "sync.h"
#include "txdb.h"
#include "util.h"
#include "util/check.h"
#include "util/system.h"
#include "utilmoneystr.h"
#include "validation.h"
#include "versionbits.h"
#include "wallet/wallet.h"
#include "warnings.h"
#include "undo.h"

#include <boost/thread/thread.hpp>

#include <condition_variable>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdint.h>
#include <univalue.h>
#include <utility>

using kernel::CCoinsStats;
using kernel::CoinStatsHashType;

struct CUpdatedBlock
{
    uint256 hash;
    int height;
};

static GlobalMutex cs_blockchange;
static std::condition_variable cond_blockchange;
static CUpdatedBlock latestblock GUARDED_BY(cs_blockchange);

void ScriptPubKeyToJSON(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex);

double GetDifficulty(const CBlockIndex* blockindex)
{
    CHECK_NONFATAL(blockindex);
    return ConvertBitsToDouble(blockindex->nBits);
}

static int ComputeNextBlockAndDepth(const CBlockIndex* tip, const CBlockIndex* blockindex, const CBlockIndex*& next)
{
    next = tip->GetAncestor(blockindex->nHeight + 1);
    if (next && next->pprev == blockindex) {
        return tip->nHeight - blockindex->nHeight + 1;
    }
    next = nullptr;
    return blockindex == tip ? 1 : -1;
}

static const CBlockIndex* ParseHashOrHeight(const UniValue& param, ChainstateManager& chainman)
{
    LOCK(::cs_main);
    CChain& active_chain = chainman.ActiveChain();

    if (param.isNum()) {
        const int height{param.getInt<int>()};

        if (height < 0) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                strprintf("Target block height %d is negative", height));
        }

        const int current_tip{active_chain.Height()};

        if (height > current_tip) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                strprintf(
                    "Target block height %d after current tip %d",
                    height,
                    current_tip));
        }

        return active_chain[height];
    } else {
        const uint256 hash{
            ParseHashV(param, "hash_or_height")};

        const CBlockIndex* pindex =
            chainman.m_blockman.LookupBlockIndex(hash);

        if (!pindex) {
            throw JSONRPCError(
                RPC_INVALID_ADDRESS_OR_KEY,
                "Block not found");
        }

        return pindex;
    }
}

UniValue blockheaderToJSON(const CBlockIndex* tip, const CBlockIndex* blockindex, const chainlock::Chainlocks& chainlocks)
{
    // Serialize passed information without accessing chain state of the active chain!
    AssertLockNotHeld(cs_main); // For performance reasons
    UniValue result(UniValue::VOBJ);
    result.pushKV("hash", blockindex->GetBlockHash().GetHex());
    const CBlockIndex* pnext;
    int confirmations = ComputeNextBlockAndDepth(tip, blockindex, pnext);
    result.pushKV("confirmations", confirmations);
    result.pushKV("height", blockindex->nHeight);
    result.pushKV("version", blockindex->nVersion);
    result.pushKV("versionHex", strprintf("%08x", blockindex->nVersion));
    result.pushKV("merkleroot", blockindex->hashMerkleRoot.GetHex());
    result.pushKV("time", (int64_t)blockindex->nTime);
    result.pushKV("mediantime", (int64_t)blockindex->GetMedianTimePast());
    result.pushKV("nonce", (uint64_t)blockindex->nNonce);
    result.pushKV("bits", strprintf("%08x", blockindex->nBits));
    result.pushKV("difficulty", GetDifficulty(blockindex));
    result.pushKV("chainwork", blockindex->nChainWork.GetHex());
    result.pushKV("nTx", (uint64_t)blockindex->nTx);

    // Historical SCH serialized block-index compatibility.
    result.pushKV("acc_checkpoint", blockindex->nAccumulatorCheckpoint.GetHex());

    if (blockindex->pprev)
        result.pushKV("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    if (pnext)
        result.pushKV("nextblockhash", pnext->GetBlockHash().GetHex());

    result.pushKV("chainlock", chainlocks.HasChainLock(blockindex->nHeight, blockindex->GetBlockHash()));
    return result;
}

UniValue blockToJSON(node::BlockManager& blockman, const CBlock& block, const CBlockIndex* tip, const CBlockIndex* blockindex, const chainlock::Chainlocks& chainlocks, const llmq::CInstantSendManager& isman, TxVerbosity verbosity)
{
    UniValue result = blockheaderToJSON(tip, blockindex, chainlocks);
    result.pushKV("size", (int)::GetSerializeSize(block, PROTOCOL_VERSION));

    UniValue txs(UniValue::VARR);
    switch (verbosity) {
    case TxVerbosity::SHOW_TXID:
        for (const CTransactionRef& tx : block.vtx) {
            txs.push_back(tx->GetHash().GetHex());
        }
        break;

    case TxVerbosity::SHOW_DETAILS:
    case TxVerbosity::SHOW_DETAILS_AND_PREVOUT: {
        CBlockUndo blockUndo;
        const bool is_not_pruned{
            WITH_LOCK(::cs_main, return !blockman.IsBlockPruned(blockindex))};
        const bool have_undo{
            is_not_pruned && node::UndoReadFromDisk(blockUndo, blockindex)};

        for (size_t i = 0; i < block.vtx.size(); ++i) {
            const CTransactionRef& tx = block.vtx.at(i);

            // The coinbase transaction has no undo entry.
            const CTxUndo* txundo =
                (have_undo && i > 0)
                    ? &blockUndo.vtxundo.at(i - 1)
                    : nullptr;

            UniValue objTx(UniValue::VOBJ);
            TxToUniv(
                *tx,
                /*block_hash=*/uint256(),
                /*entry=*/objTx,
                /*include_hex=*/true,
                /*serialize_flags=*/0,
                txundo,
                verbosity);

            const bool locked = isman.IsLocked(tx->GetHash());
            objTx.pushKV(
                "instantlock",
                locked || result["chainlock"].get_bool());
            objTx.pushKV("instantlock_internal", locked);
            txs.push_back(objTx);
        }
        break;
    }
    }

    result.pushKV("tx", txs);

    if (!block.vtx.empty() &&
        !block.vtx[0]->vExtraPayload.empty()) {
        if (const auto cbtx =
                GetTxPayload<CCbTx>(
                    block.vtx[0]->vExtraPayload)) {
            result.pushKV("cbTx", cbtx->ToJson());
        }
    }

    // Historical SCH serialized block and block-index compatibility.
    result.pushKV(
        "acc_checkpoint",
        block.nAccumulatorCheckpoint.GetHex());
    result.pushKV(
        "moneysupply",
        ValueFromAmount(blockindex->nMoneySupply));

    if (block.IsProofOfStake()) {
        uint256 hash_proof_of_stake;
        if (!GetStakeKernelHash(
                hash_proof_of_stake,
                block,
                blockindex->pprev)) {
            throw JSONRPCError(
                RPC_INTERNAL_ERROR,
                "Cannot get proof of stake hash");
        }

        const std::string stake_modifier =
            Params().GetConsensus().IsStakeModifierV2(
                blockindex->nHeight)
                ? blockindex->GetStakeModifierV2().GetHex()
                : strprintf(
                      "%016x",
                      blockindex->GetStakeModifierV1());

        result.pushKV("stakeModifier", stake_modifier);
        result.pushKV(
            "hashProofOfStake",
            hash_proof_of_stake.GetHex());
    }

    return result;
}

static RPCHelpMan getblockcount()
{
    return RPCHelpMan{"getblockcount",
                "\nReturns the height of the most-work fully-validated chain.\n"
                "The genesis block has height 0.\n",
                {},
                RPCResult{
                    RPCResult::Type::NUM, "", "The current block count"},
                RPCExamples{
                    HelpExampleCli("getblockcount", "")
            + HelpExampleRpc("getblockcount", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    return chainman.ActiveChain().Height();
},
    };
}

static RPCHelpMan getbestblockhash()
{
    return RPCHelpMan{"getbestblockhash",
                "\nReturns the hash of the best (tip) block in the most-work fully-validated chain.\n",
                {},
                RPCResult{
                    RPCResult::Type::STR_HEX, "", "the block hash, hex-encoded"},
                RPCExamples{
                    HelpExampleCli("getbestblockhash", "")
            + HelpExampleRpc("getbestblockhash", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    return chainman.ActiveChain().Tip()->GetBlockHash().GetHex();
},
    };
}

void RPCNotifyBlockChange(const CBlockIndex* pindex)
{
    if(pindex) {
        LOCK(cs_blockchange);
        latestblock.hash = pindex->GetBlockHash();
        latestblock.height = pindex->nHeight;
    }
    cond_blockchange.notify_all();
}

static RPCHelpMan waitfornewblock()
{
    return RPCHelpMan{"waitfornewblock",
                "\nWaits for a specific new block and returns useful info about it.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    {"timeout", RPCArg::Type::NUM, RPCArg::Default{0}, "Time in milliseconds to wait for a response. 0 indicates no timeout."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "hash", "The blockhash"},
                        {RPCResult::Type::NUM, "height", "Block height"},
                    }},
                RPCExamples{
                    HelpExampleCli("waitfornewblock", "1000")
            + HelpExampleRpc("waitfornewblock", "1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    int timeout = 0;
    if (!request.params[0].isNull())
        timeout = request.params[0].getInt<int>();

    CUpdatedBlock block;
    {
        WAIT_LOCK(cs_blockchange, lock);
        block = latestblock;
        if(timeout)
            cond_blockchange.wait_for(lock, std::chrono::milliseconds(timeout), [&block]() EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.height != block.height || latestblock.hash != block.hash || !IsRPCRunning(); });
        else
            cond_blockchange.wait(lock, [&block]() EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.height != block.height || latestblock.hash != block.hash || !IsRPCRunning(); });
        block = latestblock;
    }

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("hash", block.hash.GetHex());
    ret.pushKV("height", block.height);
    return ret;
},
    };
}

static RPCHelpMan waitforblock()
{
    return RPCHelpMan{"waitforblock",
                "\nWaits for a specific new block and returns useful info about it.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Block hash to wait for."},
                    {"timeout", RPCArg::Type::NUM, RPCArg::Default{0}, "Time in milliseconds to wait for a response. 0 indicates no timeout."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "hash", "The blockhash"},
                        {RPCResult::Type::NUM, "height", "Block height"},
                    }},
                RPCExamples{
                    HelpExampleCli("waitforblock", "\"0000000000079f8ef3d2c688c244eb7a4570b24c9ed7b4a8c619eb02596f8862\" 1000")
            + HelpExampleRpc("waitforblock", "\"0000000000079f8ef3d2c688c244eb7a4570b24c9ed7b4a8c619eb02596f8862\", 1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    int timeout = 0;

    uint256 hash(ParseHashV(request.params[0], "blockhash"));

    if (!request.params[1].isNull())
        timeout = request.params[1].getInt<int>();

    CUpdatedBlock block;
    {
        WAIT_LOCK(cs_blockchange, lock);

        if(timeout)
            cond_blockchange.wait_for(lock, std::chrono::milliseconds(timeout),  EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.hash == hash || !IsRPCRunning();});
        else
            cond_blockchange.wait(lock,  EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.hash == hash || !IsRPCRunning(); });

        block = latestblock;
    }

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("hash", block.hash.GetHex());
    ret.pushKV("height", block.height);
    return ret;
},
    };
}

static RPCHelpMan waitforblockheight()
{
    return RPCHelpMan{"waitforblockheight",
                "\nWaits for (at least) block height and returns the height and hash\n"
                "of the current tip.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "Block height to wait for."},
                    {"timeout", RPCArg::Type::NUM, RPCArg::Default{0}, "Time in milliseconds to wait for a response. 0 indicates no timeout."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "hash", "The blockhash"},
                        {RPCResult::Type::NUM, "height", "Block height"},
                    }},
                RPCExamples{
                    HelpExampleCli("waitforblockheight", "100 1000")
            + HelpExampleRpc("waitforblockheight", "100, 1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    int timeout = 0;

    int height = request.params[0].getInt<int>();

    if (!request.params[1].isNull())
        timeout = request.params[1].getInt<int>();

    CUpdatedBlock block;
    {
        WAIT_LOCK(cs_blockchange, lock);

        if(timeout)
            cond_blockchange.wait_for(lock, std::chrono::milliseconds(timeout),  EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.height >= height || !IsRPCRunning();});
        else
            cond_blockchange.wait(lock,  EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.height >= height || !IsRPCRunning(); });

        block = latestblock;
    }

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("hash", block.hash.GetHex());
    ret.pushKV("height", block.height);
    return ret;
},
    };
}

static RPCHelpMan getdifficulty()
{
    return RPCHelpMan{"getdifficulty",
                "\nReturns the proof-of-work difficulty as a multiple of the minimum difficulty.\n",
                {},
                RPCResult{
                    RPCResult::Type::NUM, "", "the proof-of-work difficulty as a multiple of the minimum difficulty."},
                RPCExamples{
                    HelpExampleCli("getdifficulty", "")
            + HelpExampleRpc("getdifficulty", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    return GetDifficulty(chainman.ActiveChain().Tip());
},
    };
}

static RPCHelpMan getblockhash()
{
    return RPCHelpMan{"getblockhash",
                "\nReturns hash of block in best-block-chain at height provided.\n",
                {
                    {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "The height index"},
                },
                RPCResult{
                    RPCResult::Type::STR_HEX, "", "The block hash"},
                RPCExamples{
                    HelpExampleCli("getblockhash", "1000")
            + HelpExampleRpc("getblockhash", "1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    const CChain& active_chain = chainman.ActiveChain();
    int nHeight = request.params[0].getInt<int>();
    if (nHeight < 0 || nHeight > active_chain.Height())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
    const CBlockIndex* pblockindex = active_chain[nHeight];
    return pblockindex->GetBlockHash().GetHex();
},
    };
}

static RPCHelpMan getblock()
{
    return RPCHelpMan{"getblock",
                "\nIf verbosity is 0, returns serialized, hex-encoded block data.\n"
                "If verbosity is 1, returns an Object with information about the block.\n"
                "If verbosity is 2, returns an Object with information about the block and each transaction.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The block hash"},
                    {"verbosity|verbose", RPCArg::Type::NUM, RPCArg::Default{1}, "0 for hex-encoded data, 1 for a JSON object, and 2 for a JSON object with transaction data",
                     RPCArgOptions{.skip_type_check = true}},
                },
                {
                    RPCResult{"for verbosity = 0",
                        RPCResult::Type::STR_HEX, "", "The serialized, hex-encoded block data"},
                    RPCResult{"for verbosity = 1",
                        RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR_HEX, "hash", "The block hash"},
                            {RPCResult::Type::NUM, "confirmations", "The number of confirmations, or -1 if the block is not on the main chain"},
                            {RPCResult::Type::NUM, "size", "The block size"},
                            {RPCResult::Type::NUM, "height", "The block height"},
                            {RPCResult::Type::NUM, "version", "The block version"},
                            {RPCResult::Type::STR_HEX, "merkleroot", "The merkle root"},
                            {RPCResult::Type::STR_HEX, "acc_checkpoint", "The historical SCH accumulator checkpoint"},
                            {RPCResult::Type::ARR, "tx", "The transaction ids",
                                {{RPCResult::Type::STR_HEX, "", "The transaction id"}}},
                            {RPCResult::Type::NUM_TIME, "time", "The block time"},
                            {RPCResult::Type::NUM_TIME, "mediantime", "The median block time"},
                            {RPCResult::Type::NUM, "nonce", "The nonce"},
                            {RPCResult::Type::STR_HEX, "bits", "The bits"},
                            {RPCResult::Type::NUM, "difficulty", "The difficulty"},
                            {RPCResult::Type::STR_HEX, "chainwork", "The chain work"},
                            {RPCResult::Type::STR_HEX, "previousblockhash", /*optional=*/true, "The previous block hash"},
                            {RPCResult::Type::STR_HEX, "nextblockhash", /*optional=*/true, "The next block hash"},
                            {RPCResult::Type::NUM, "moneysupply", "The SCH money supply at this block"},
                            {RPCResult::Type::STR_HEX, "stakeModifier", /*optional=*/true, "The proof-of-stake modifier"},
                            {RPCResult::Type::STR_HEX, "hashProofOfStake", /*optional=*/true, "The proof-of-stake kernel hash"},
                        }},
                    RPCResult{"for verbosity = 2",
                        RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::ELISION, "", "Same output as verbosity = 1"},
                            {RPCResult::Type::ARR, "tx", "The transactions",
                                {
                                    {RPCResult::Type::OBJ, "", "",
                                        {
                                            {RPCResult::Type::ELISION, "", "The transaction in JSON format"},
                                        }},
                                }},
                        }},
                },
                RPCExamples{
                    HelpExampleCli("getblock", "\"00000000000fd08c2fb661d2fcb0d49abb3a91e5f27082ce64feed3b4dede2e2\" 1")
            + HelpExampleCli("getblock", "\"00000000000fd08c2fb661d2fcb0d49abb3a91e5f27082ce64feed3b4dede2e2\" 2")
            + HelpExampleRpc("getblock", "\"00000000000fd08c2fb661d2fcb0d49abb3a91e5f27082ce64feed3b4dede2e2\", 2")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const node::NodeContext& node = EnsureAnyNodeContext(request.context);
    ChainstateManager& chainman = EnsureChainman(node);

    const uint256 hash = ParseHashV(request.params[0], "blockhash");

    int verbosity = 1;
    if (!request.params[1].isNull()) {
        if (request.params[1].isBool()) {
            verbosity = request.params[1].get_bool() ? 1 : 0;
        } else {
            verbosity = request.params[1].getInt<int>();
        }
    }

    if (verbosity < 0 || verbosity > 2) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Verbosity must be 0, 1, or 2");
    }

    const CBlockIndex* pblockindex;
    const CBlockIndex* tip;
    {
        LOCK(cs_main);
        pblockindex = chainman.m_blockman.LookupBlockIndex(hash);
        tip = chainman.ActiveChain().Tip();
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    }

    CBlock block;
    if (!node::ReadBlockFromDisk(block, pblockindex, chainman.GetConsensus())) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");
    }

    if (verbosity == 0) {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
        return HexStr(ssBlock.begin(), ssBlock.end());
    }

    llmq::CInstantSendManager& isman = EnsureInstantSendManager(node);
    CHECK_NONFATAL(node.chainlocks);

    const TxVerbosity tx_verbosity =
        verbosity == 2
            ? TxVerbosity::SHOW_DETAILS
            : TxVerbosity::SHOW_TXID;

    return blockToJSON(
        chainman.m_blockman,
        block,
        tip,
        pblockindex,
        *node.chainlocks,
        isman,
        tx_verbosity);
},
    };
}

static RPCHelpMan getblockheader()
{
    return RPCHelpMan{"getblockheader",
                "\nIf verbose is false, returns a string that is serialized, hex-encoded data for blockheader 'hash'.\n"
                "If verbose is true, returns an Object with information about blockheader <hash>.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The block hash"},
                    {"verbose", RPCArg::Type::BOOL, RPCArg::Default{true}, "true for a json object, false for the hex-encoded data"},
                },
                {
                    RPCResult{"for verbose = true",
                        RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR_HEX, "hash", "the block hash (same as provided)"},
                            {RPCResult::Type::NUM, "confirmations", "The number of confirmations, or -1 if the block is not on the main chain"},
                            {RPCResult::Type::NUM, "height", "The block height or index"},
                            {RPCResult::Type::NUM, "version", "The block version"},
                            {RPCResult::Type::STR_HEX, "versionHex", "The block version formatted in hexadecimal"},
                            {RPCResult::Type::STR_HEX, "merkleroot", "The merkle root"},
                            {RPCResult::Type::NUM_TIME, "time", "The block time expressed in " + UNIX_EPOCH_TIME},
                            {RPCResult::Type::NUM_TIME, "mediantime", "The median block time expressed in " + UNIX_EPOCH_TIME},
                            {RPCResult::Type::NUM, "nonce", "The nonce"},
                            {RPCResult::Type::STR_HEX, "bits", "The bits"},
                            {RPCResult::Type::NUM, "difficulty", "The difficulty"},
                            {RPCResult::Type::STR_HEX, "chainwork", "Expected number of hashes required to produce the current chain"},
                            {RPCResult::Type::NUM, "nTx", "The number of transactions in the block"},
                            {RPCResult::Type::STR_HEX, "acc_checkpoint", "The historical SCH accumulator checkpoint"},
                            {RPCResult::Type::STR_HEX, "previousblockhash", /*optional=*/true, "The hash of the previous block (if available)"},
                            {RPCResult::Type::STR_HEX, "nextblockhash", /*optional=*/true, "The hash of the next block (if available)"},
                            {RPCResult::Type::BOOL, "chainlock", "The state of the block ChainLock"},
                        }},
                    RPCResult{"for verbose=false",
                        RPCResult::Type::STR_HEX, "", "A string that is serialized, hex-encoded data for block 'hash'"},
                },
                RPCExamples{
                    HelpExampleCli("getblockheader", "\"00000000000fd08c2fb661d2fcb0d49abb3a91e5f27082ce64feed3b4dede2e2\"")
            + HelpExampleRpc("getblockheader", "\"00000000000fd08c2fb661d2fcb0d49abb3a91e5f27082ce64feed3b4dede2e2\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    uint256 hash(ParseHashV(request.params[0], "hash"));
    const node::NodeContext& node = EnsureAnyNodeContext(request.context);

    bool fVerbose = true;
    if (!request.params[1].isNull())
        fVerbose = request.params[1].get_bool();

    const CBlockIndex* pblockindex;
    const CBlockIndex* tip;
    {
        ChainstateManager& chainman = EnsureChainman(node);
        LOCK(cs_main);
        pblockindex = chainman.m_blockman.LookupBlockIndex(hash);
        tip = chainman.ActiveChain().Tip();
    }

    if (!pblockindex) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
    }

    if (!fVerbose)
    {
        DataStream ssBlock{};
        ssBlock << pblockindex->GetBlockHeader();
        std::string strHex = HexStr(ssBlock);
        return strHex;
    }

    CHECK_NONFATAL(node.chainlocks);
    return blockheaderToJSON(tip, pblockindex, *node.chainlocks);
},
    };
}

CoinStatsHashType ParseHashType(const std::string& hash_type_input)
{
    if (hash_type_input == "hash_serialized_2") {
        return CoinStatsHashType::HASH_SERIALIZED;
    } else if (hash_type_input == "muhash") {
        return CoinStatsHashType::MUHASH;
    } else if (hash_type_input == "none") {
        return CoinStatsHashType::NONE;
    } else {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            strprintf(
                "'%s' is not a valid hash_type",
                hash_type_input));
    }
}

std::optional<kernel::CCoinsStats> GetUTXOStats(
    CCoinsView* view,
    node::BlockManager& blockman,
    kernel::CoinStatsHashType hash_type,
    const std::function<void()>& interruption_point,
    const CBlockIndex* pindex,
    bool index_requested)
{
    // Use CoinStatsIndex if it is requested and available and a hash_type
    // of Muhash or None was requested.
    if ((hash_type == kernel::CoinStatsHashType::MUHASH ||
         hash_type == kernel::CoinStatsHashType::NONE) &&
        g_coin_stats_index &&
        index_requested) {
        if (pindex) {
            return g_coin_stats_index->LookUpStats(*pindex);
        } else {
            CBlockIndex& block_index =
                *CHECK_NONFATAL(
                    WITH_LOCK(
                        ::cs_main,
                        return blockman.LookupBlockIndex(
                            view->GetBestBlock())));

            return g_coin_stats_index->LookUpStats(block_index);
        }
    }

    // Without the CoinStats index, statistics can only be calculated for
    // the best block represented by the supplied coins view.
    CHECK_NONFATAL(
        !pindex ||
        pindex->GetBlockHash() == view->GetBestBlock());

    return kernel::ComputeUTXOStats(
        hash_type,
        view,
        blockman,
        interruption_point);
}

static RPCHelpMan gettxoutsetinfo()
{
    return RPCHelpMan{"gettxoutsetinfo",
                "\nReturns statistics about the unspent transaction output set.\n"
                "Note this call may take some time if you are not using coinstatsindex.\n",
                {
                    {"hash_type", RPCArg::Type::STR, RPCArg::Default{"hash_serialized_2"}, "Which UTXO set hash should be calculated. Options: 'hash_serialized_2' (the legacy algorithm), 'muhash', 'none'."},
                    {"hash_or_height", RPCArg::Type::NUM, RPCArg::DefaultHint{"the current best block"}, "The block hash or height of the target height (only available with coinstatsindex).",
                     RPCArgOptions{
                         .skip_type_check = true,
                         .type_str = {"", "string or numeric"},
                     }},
                    {"use_index", RPCArg::Type::BOOL, RPCArg::Default{true}, "Use coinstatsindex, if available."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "height", "The block height (index) of the returned statistics"},
                        {RPCResult::Type::STR_HEX, "bestblock", "The hash of the block at which these statistics are calculated"},
                        {RPCResult::Type::NUM, "txouts", "The number of unspent transaction outputs"},
                        {RPCResult::Type::NUM, "bogosize", "Database-independent, meaningless metric indicating the UTXO set size"},
                        {RPCResult::Type::STR_HEX, "hash_serialized_2", /*optional=*/true, "The serialized hash (only present if 'hash_serialized_2' hash_type is chosen)"},
                        {RPCResult::Type::STR_HEX, "muhash", /*optional=*/true, "The serialized hash (only present if 'muhash' hash_type is chosen)"},
                        {RPCResult::Type::NUM, "transactions", /*optional=*/true, "The number of transactions with unspent outputs (not available when coinstatsindex is used)"},
                        {RPCResult::Type::NUM, "disk_size", /*optional=*/true, "The estimated size of the chainstate on disk (not available when coinstatsindex is used)"},
                        {RPCResult::Type::STR_AMOUNT, "total_amount", "The total amount of coins in the UTXO set"},
                        {RPCResult::Type::STR_AMOUNT, "total_unspendable_amount", /*optional=*/true, "The total amount of coins permanently excluded from the UTXO set (only available if coinstatsindex is used)"},
                        {RPCResult::Type::OBJ, "block_info", /*optional=*/true, "Info on amounts in the block at this block height (only available if coinstatsindex is used)",
                        {
                            {RPCResult::Type::STR_AMOUNT, "prevout_spent", "Total amount of all prevouts spent in this block"},
                            {RPCResult::Type::STR_AMOUNT, "coinbase", "Coinbase subsidy amount of this block"},
                            {RPCResult::Type::STR_AMOUNT, "new_outputs_ex_coinbase", "Total amount of new outputs created by this block"},
                            {RPCResult::Type::STR_AMOUNT, "unspendable", "Total amount of unspendable outputs created in this block"},
                            {RPCResult::Type::OBJ, "unspendables", "Detailed view of the unspendable categories",
                            {
                                {RPCResult::Type::STR_AMOUNT, "genesis_block", "The unspendable amount of the Genesis block subsidy"},
                                {RPCResult::Type::STR_AMOUNT, "scripts", "Amounts sent to scripts that are unspendable (for example OP_RETURN outputs)"},
                                {RPCResult::Type::STR_AMOUNT, "unclaimed_rewards", "Fee rewards that were not claimed in the block reward"},
                            }}
                        }},
                    }},
                RPCExamples{
                    HelpExampleCli("gettxoutsetinfo", "") +
                    HelpExampleCli("gettxoutsetinfo", R"("none")") +
                    HelpExampleCli("gettxoutsetinfo", R"("none" 1000)") +
                    HelpExampleCli("gettxoutsetinfo", R"("none" '"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09"')") +
                    HelpExampleCli("-named gettxoutsetinfo", R"(hash_type='muhash' use_index='false')") +
                    HelpExampleRpc("gettxoutsetinfo", "") +
                    HelpExampleRpc("gettxoutsetinfo", R"("none")") +
                    HelpExampleRpc("gettxoutsetinfo", R"("none", 1000)") +
                    HelpExampleRpc("gettxoutsetinfo", R"("none", "00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09")")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    UniValue ret(UniValue::VOBJ);
    const CBlockIndex* pindex{nullptr};

    const CoinStatsHashType hash_type{
        request.params[0].isNull()
            ? CoinStatsHashType::HASH_SERIALIZED
            : ParseHashType(request.params[0].get_str())};

    bool index_requested =
        request.params[2].isNull() ||
        request.params[2].get_bool();

    const node::NodeContext& node =
        EnsureAnyNodeContext(request.context);

    ChainstateManager& chainman =
        EnsureChainman(node);

    Chainstate& active_chainstate =
        chainman.ActiveChainstate();

    active_chainstate.ForceFlushStateToDisk();

    CCoinsView* coins_view;
    node::BlockManager* blockman;

    {
        LOCK(::cs_main);
        coins_view = &active_chainstate.CoinsDB();
        blockman = &active_chainstate.m_blockman;
        pindex =
            blockman->LookupBlockIndex(
                coins_view->GetBestBlock());
    }

    if (!request.params[1].isNull()) {
        if (!g_coin_stats_index) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Querying specific block heights requires coinstatsindex");
        }

        if (hash_type == CoinStatsHashType::HASH_SERIALIZED) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "hash_serialized_2 hash type cannot be queried for a specific block");
        }

        if (!index_requested) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Cannot set use_index to false when querying for a specific block");
        }

        pindex =
            ParseHashOrHeight(
                request.params[1],
                chainman);
    }

    if (index_requested && g_coin_stats_index) {
        if (!g_coin_stats_index->BlockUntilSyncedToCurrentChain()) {
            const IndexSummary summary{
                g_coin_stats_index->GetSummary()};

            if (pindex->nHeight > summary.best_block_height) {
                throw JSONRPCError(
                    RPC_INTERNAL_ERROR,
                    strprintf(
                        "Unable to get data because coinstatsindex is still syncing. Current height: %d",
                        summary.best_block_height));
            }
        }
    }

    const std::optional<CCoinsStats> maybe_stats =
        GetUTXOStats(
            coins_view,
            *blockman,
            hash_type,
            node.rpc_interruption_point,
            pindex,
            index_requested);

    if (maybe_stats.has_value()) {
        const CCoinsStats& stats = maybe_stats.value();

        ret.pushKV("height", (int64_t)stats.nHeight);
        ret.pushKV("bestblock", stats.hashBlock.GetHex());
        ret.pushKV("txouts", (int64_t)stats.nTransactionOutputs);
        ret.pushKV("bogosize", (int64_t)stats.nBogoSize);

        if (hash_type == CoinStatsHashType::HASH_SERIALIZED) {
            ret.pushKV(
                "hash_serialized_2",
                stats.hashSerialized.GetHex());
        }

        if (hash_type == CoinStatsHashType::MUHASH) {
            ret.pushKV(
                "muhash",
                stats.hashSerialized.GetHex());
        }

        CHECK_NONFATAL(stats.total_amount.has_value());

        ret.pushKV(
            "total_amount",
            ValueFromAmount(stats.total_amount.value()));

        if (!stats.index_used) {
            ret.pushKV(
                "transactions",
                static_cast<int64_t>(stats.nTransactions));
            ret.pushKV("disk_size", stats.nDiskSize);
        } else {
            ret.pushKV(
                "total_unspendable_amount",
                ValueFromAmount(
                    stats.total_unspendable_amount));

            CCoinsStats prev_stats{};

            if (pindex->nHeight > 0) {
                const std::optional<CCoinsStats> maybe_prev_stats =
                    GetUTXOStats(
                        coins_view,
                        *blockman,
                        hash_type,
                        node.rpc_interruption_point,
                        pindex->pprev,
                        index_requested);

                if (!maybe_prev_stats) {
                    throw JSONRPCError(
                        RPC_INTERNAL_ERROR,
                        "Unable to read UTXO set");
                }

                prev_stats = maybe_prev_stats.value();
            }

            UniValue block_info(UniValue::VOBJ);

            block_info.pushKV(
                "prevout_spent",
                ValueFromAmount(
                    stats.total_prevout_spent_amount -
                    prev_stats.total_prevout_spent_amount));

            block_info.pushKV(
                "coinbase",
                ValueFromAmount(
                    stats.total_coinbase_amount -
                    prev_stats.total_coinbase_amount));

            block_info.pushKV(
                "new_outputs_ex_coinbase",
                ValueFromAmount(
                    stats.total_new_outputs_ex_coinbase_amount -
                    prev_stats.total_new_outputs_ex_coinbase_amount));

            block_info.pushKV(
                "unspendable",
                ValueFromAmount(
                    stats.total_unspendable_amount -
                    prev_stats.total_unspendable_amount));

            UniValue unspendables(UniValue::VOBJ);

            unspendables.pushKV(
                "genesis_block",
                ValueFromAmount(
                    stats.total_unspendables_genesis_block -
                    prev_stats.total_unspendables_genesis_block));

            unspendables.pushKV(
                "scripts",
                ValueFromAmount(
                    stats.total_unspendables_scripts -
                    prev_stats.total_unspendables_scripts));

            unspendables.pushKV(
                "unclaimed_rewards",
                ValueFromAmount(
                    stats.total_unspendables_unclaimed_rewards -
                    prev_stats.total_unspendables_unclaimed_rewards));

            block_info.pushKV(
                "unspendables",
                unspendables);

            ret.pushKV(
                "block_info",
                block_info);
        }
    } else {
        throw JSONRPCError(
            RPC_INTERNAL_ERROR,
            "Unable to read UTXO set");
    }

    return ret;
},
    };
}

static RPCHelpMan gettxout()
{
    return RPCHelpMan{"gettxout",
        "\nReturns details about an unspent transaction output.\n",
        {
            {"txid", RPCArg::Type::STR, RPCArg::Optional::NO, "The transaction id"},
            {"n", RPCArg::Type::NUM, RPCArg::Optional::NO, "vout number"},
            {"include_mempool", RPCArg::Type::BOOL, RPCArg::Default{true}, "Whether to include the mempool. Note that an unspent output that is spent in the mempool won't appear."},
        },
        {
            RPCResult{"If the UTXO was not found", RPCResult::Type::NONE, "", ""},
            RPCResult{"Otherwise", RPCResult::Type::OBJ, "", "", {
                {RPCResult::Type::STR_HEX, "bestblock", "The hash of the block at the tip of the chain"},
                {RPCResult::Type::NUM, "confirmations", "The number of confirmations"},
                {RPCResult::Type::STR_AMOUNT, "value", "The transaction value in " + CURRENCY_UNIT},
                {RPCResult::Type::OBJ, "scriptPubKey", "", {
                    {RPCResult::Type::STR, "asm", "Disassembly of the public key script"},
                    {RPCResult::Type::STR, "desc", "Inferred descriptor for the output"},
                    {RPCResult::Type::STR_HEX, "hex", "The raw public key script bytes, hex-encoded"},
                    {RPCResult::Type::STR_HEX, "type", "The type, eg pubkeyhash"},
                    {RPCResult::Type::STR, "address", /*optional=*/ true, "SchillingCoin address (only if a well-defined address exists)"},
                }},
                {RPCResult::Type::BOOL, "coinbase", "Coinbase or not"},
            }},
        },
        RPCExamples{
            "\nGet unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nView the details\n"
            + HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("gettxout", "\"txid\", 1")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const node::NodeContext& node = EnsureAnyNodeContext(request.context);
    ChainstateManager& chainman = EnsureChainman(node);

    LOCK(cs_main);

    Chainstate& active_chainstate = chainman.ActiveChainstate();
    UniValue ret(UniValue::VOBJ);

    uint256 hash(ParseHashV(request.params[0], "txid"));
    COutPoint out{hash, request.params[1].getInt<uint32_t>()};

    bool fMempool = true;
    if (!request.params[2].isNull())
        fMempool = request.params[2].get_bool();

    Coin coin;
    CCoinsViewCache* coins_view = &active_chainstate.CoinsTip();

    if (fMempool) {
        const CTxMemPool& mempool = EnsureMemPool(node);
        LOCK(mempool.cs);
        CCoinsViewMemPool view(coins_view, mempool);

        if (!view.GetCoin(out, coin) || mempool.isSpent(out)) {
            return UniValue::VNULL;
        }
    } else {
        if (!coins_view->GetCoin(out, coin)) {
            return UniValue::VNULL;
        }
    }

    const CBlockIndex* pindex =
        active_chainstate.m_blockman.LookupBlockIndex(
            coins_view->GetBestBlock());

    ret.pushKV("bestblock", pindex->GetBlockHash().GetHex());

    if (coin.nHeight == MEMPOOL_HEIGHT) {
        ret.pushKV("confirmations", 0);
    } else {
        ret.pushKV(
            "confirmations",
            (int64_t)(pindex->nHeight - coin.nHeight + 1));
    }

    ret.pushKV("value", ValueFromAmount(coin.out.nValue));

    UniValue o(UniValue::VOBJ);
    ScriptToUniv(
        coin.out.scriptPubKey,
        /*out=*/o,
        /*include_hex=*/true,
        /*include_address=*/true);

    ret.pushKV("scriptPubKey", o);
    ret.pushKV("coinbase", (bool)coin.fCoinBase);

    return ret;
},
    };
}

static RPCHelpMan verifychain()
{
    return RPCHelpMan{
        "verifychain",
        "\nVerifies blockchain database.\n",
        {
            {"checklevel", RPCArg::Type::NUM,
                RPCArg::DefaultHint{strprintf("%d, range=0-4", DEFAULT_CHECKLEVEL)},
                strprintf("How thorough the block verification is:\n - %s",
                    MakeUnorderedList(CHECKLEVEL_DOC))},
            {"nblocks", RPCArg::Type::NUM,
                RPCArg::DefaultHint{strprintf("%d, 0=all", DEFAULT_CHECKBLOCKS)},
                "The number of blocks to check."},
        },
        RPCResult{
            RPCResult::Type::BOOL,
            "",
            "Verification finished successfully. If false, check debug.log for the reason.",
        },
        RPCExamples{
            HelpExampleCli("verifychain", "") +
            HelpExampleRpc("verifychain", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            const int check_level{
                request.params[0].isNull()
                    ? DEFAULT_CHECKLEVEL
                    : request.params[0].getInt<int>()};

            const int check_depth{
                request.params[1].isNull()
                    ? DEFAULT_CHECKBLOCKS
                    : request.params[1].getInt<int>()};

            const node::NodeContext& node =
                EnsureAnyNodeContext(request.context);

            ChainstateManager& chainman =
                EnsureChainman(node);

            LOCK(cs_main);

            Chainstate& active_chainstate =
                chainman.ActiveChainstate();

            return CVerifyDB().VerifyDB(
                       active_chainstate,
                       chainman.GetParams().GetConsensus(),
                       active_chainstate.CoinsTip(),
                       *CHECK_NONFATAL(node.evodb),
                       check_level,
                       check_depth) == VerifyDBResult::SUCCESS;
        },
    };
}

/** Implementation of IsSuperMajority with better feedback */
static void SoftForkDescPushBack(const CBlockIndex* blockindex, UniValue& softforks, const ChainstateManager& chainman, Consensus::BuriedDeployment dep)
{
    // For buried deployments.
    if (!DeploymentEnabled(chainman, dep)) return;
    UniValue rv(UniValue::VOBJ);
    rv.pushKV("type", "buried");
    // getdeploymentinfo reports the softfork as active from when the chain height is
    // one below the activation height
    rv.pushKV("active", DeploymentActiveAfter(blockindex, chainman.GetConsensus(), dep));
    rv.pushKV("height", chainman.GetConsensus().DeploymentHeight(dep));
    softforks.pushKV(DeploymentName(dep), rv);
}

static void SoftForkDescPushBack(const CBlockIndex* blockindex, const AbstractEHFManager::Signals& signals, UniValue& softforks, const ChainstateManager& chainman, Consensus::DeploymentPos id)
{
    // For BIP9 deployments.
    if (!DeploymentEnabled(chainman, id)) return;
    if (blockindex == nullptr) return;
    auto get_state_name = [](const ThresholdState state) -> std::string {
        switch (state) {
        case ThresholdState::DEFINED: return "defined";
        case ThresholdState::STARTED: return "started";
        case ThresholdState::LOCKED_IN: return "locked_in";
        case ThresholdState::ACTIVE: return "active";
        case ThresholdState::FAILED: return "failed";
        }
        return "invalid";
    };
    UniValue bip9(UniValue::VOBJ);
    const ThresholdState next_state = chainman.m_versionbitscache.State(blockindex, chainman.GetConsensus(), id);
    const ThresholdState current_state = chainman.m_versionbitscache.State(blockindex->pprev, chainman.GetConsensus(), id);
    const bool has_signal = (ThresholdState::STARTED == current_state || ThresholdState::LOCKED_IN == current_state);
    // BIP9 parameters
    if (has_signal) {
        bip9.pushKV("bit", chainman.GetConsensus().vDeployments[id].bit);
    }
    bip9.pushKV("start_time", chainman.GetConsensus().vDeployments[id].nStartTime);
    bip9.pushKV("timeout", chainman.GetConsensus().vDeployments[id].nTimeout);
    bip9.pushKV("ehf", chainman.GetConsensus().vDeployments[id].useEHF);
    if (auto it = signals.find(chainman.GetConsensus().vDeployments[id].bit); it != signals.end()) {
        bip9.pushKV("ehf_height", it->second);
    }
    int64_t since_height = chainman.m_versionbitscache.StateSinceHeight(blockindex->pprev, chainman.GetConsensus(), id);
    if (ThresholdState::LOCKED_IN == current_state) {
        bip9.pushKV("activation_height", since_height + static_cast<int>(chainman.GetConsensus().vDeployments[id].nWindowSize));
    }
    bip9.pushKV("min_activation_height", chainman.GetConsensus().vDeployments[id].min_activation_height);
    // BIP9 status
    bip9.pushKV("status", get_state_name(current_state));
    bip9.pushKV("since", since_height);
    bip9.pushKV("status_next", get_state_name(next_state));
    // BIP9 signalling status, if applicable
    if (has_signal) {
        UniValue statsUV(UniValue::VOBJ);
        std::vector<bool> signals;
        BIP9Stats statsStruct = chainman.m_versionbitscache.Statistics(blockindex, chainman.GetConsensus(), id, &signals);
        statsUV.pushKV("period", statsStruct.period);
        statsUV.pushKV("elapsed", statsStruct.elapsed);
        statsUV.pushKV("count", statsStruct.count);
        if (ThresholdState::LOCKED_IN != current_state) {
            statsUV.pushKV("threshold", statsStruct.threshold);
            statsUV.pushKV("possible", statsStruct.possible);
        }
        bip9.pushKV("statistics", statsUV);
        std::string sig;
        sig.reserve(signals.size());
        for (const bool s : signals) {
            sig.push_back(s ? '#' : '-');
        }
        bip9.pushKV("signalling", sig);
    }
    UniValue rv(UniValue::VOBJ);
    rv.pushKV("type", "bip9");
    if (ThresholdState::ACTIVE == next_state) {
        rv.pushKV("height", chainman.m_versionbitscache.StateSinceHeight(blockindex, chainman.GetConsensus(), id));
    }
    rv.pushKV("active", ThresholdState::ACTIVE == next_state);
    rv.pushKV("bip9", bip9);
    softforks.pushKV(DeploymentName(id), rv);
}

const std::vector<RPCResult> RPCHelpForDeployment{
    {RPCResult::Type::STR, "type", "one of \"buried\", \"bip9\""},
    {RPCResult::Type::NUM, "height", /*optional=*/true, "height of the first block which the rules are or will be enforced (only for \"buried\" type, or \"bip9\" type with \"active\" status)"},
    {RPCResult::Type::BOOL, "active", "true if the rules are enforced for the mempool and the next block"},
    {RPCResult::Type::OBJ, "bip9", /*optional=*/true, "status of bip9 softforks (only for \"bip9\" type)",
    {
        {RPCResult::Type::NUM, "bit", /*optional=*/true, "the bit (0-28) in the block version field used to signal this softfork (only for \"started\" and \"locked_in\" status)"},
        {RPCResult::Type::NUM_TIME, "start_time", "the minimum median time past of a block at which the bit gains its meaning"},
        {RPCResult::Type::NUM_TIME, "timeout", "the median time past of a block at which the deployment is considered failed if not yet locked in"},
        {RPCResult::Type::BOOL, "ehf", "returns true for EHF activated forks"},
        {RPCResult::Type::NUM, "ehf_height", /*optional=*/true, "the minimum height when signals for the deployment matter. Below this height signaling cannot trigger hard fork lock-in. Not specified for non-EHF forks"},
        {RPCResult::Type::NUM, "activation_height", /*optional=*/true, "expected activation height for this softfork (only for \"locked_in\" status)"},
        {RPCResult::Type::NUM, "min_activation_height", "minimum height of blocks for which the rules may be enforced"},
        {RPCResult::Type::STR, "status", "status of deployment at specified block (one of \"defined\", \"started\", \"locked_in\", \"active\", \"failed\")"},
        {RPCResult::Type::NUM, "since", "height of the first block to which the status applies"},
        {RPCResult::Type::STR, "status_next", "status of deployment at the next block"},
        {RPCResult::Type::OBJ, "statistics", /*optional=*/true, "numeric statistics about signalling for a softfork (only for \"started\" and \"locked_in\" status)",
        {
            {RPCResult::Type::NUM, "period", "the length in blocks of the signalling period"},
            {RPCResult::Type::NUM, "threshold", /*optional=*/true, "the number of blocks with the version bit set required to activate the feature (only for \"started\" status)"},
            {RPCResult::Type::NUM, "elapsed", "the number of blocks elapsed since the beginning of the current period"},
            {RPCResult::Type::NUM, "count", "the number of blocks with the version bit set in the current period"},
            {RPCResult::Type::BOOL, "possible", /*optional=*/true, "returns false if there are not enough blocks left in this period to pass activation threshold (only for \"started\" status)"},
        }},
        {RPCResult::Type::STR, "signalling", /*optional=*/true, "indicates blocks that signalled with a # and blocks that did not with a -"},
    }},
};

UniValue DeploymentInfo(const CBlockIndex* blockindex, const AbstractEHFManager::Signals& ehf_signals, const ChainstateManager& chainman)
{
    UniValue softforks(UniValue::VOBJ);

    for (auto deploy : { /* sorted by activation block */
                         Consensus::DEPLOYMENT_HEIGHTINCB,
                         Consensus::DEPLOYMENT_DERSIG,
                         Consensus::DEPLOYMENT_CLTV,
                         Consensus::DEPLOYMENT_BIP147,
                         Consensus::DEPLOYMENT_CSV,
                         Consensus::DEPLOYMENT_DIP0001,
                         Consensus::DEPLOYMENT_DIP0003,
                         Consensus::DEPLOYMENT_DIP0008,
                         Consensus::DEPLOYMENT_DIP0020,
                         Consensus::DEPLOYMENT_DIP0024,
                         Consensus::DEPLOYMENT_BRR,
                         Consensus::DEPLOYMENT_V19,
                         Consensus::DEPLOYMENT_V20,
                         Consensus::DEPLOYMENT_MN_RR,
                        }) {
        SoftForkDescPushBack(blockindex, softforks, chainman, deploy);
    }

    for (auto ehf_deploy : { /* sorted by activation block */
                             Consensus::DEPLOYMENT_TESTDUMMY }) {
        SoftForkDescPushBack(blockindex, ehf_signals, softforks, chainman, ehf_deploy);
    }

    return softforks;
}

RPCHelpMan getblockchaininfo()
{
    /* TODO: remove -deprecatedrpc=softforks when no longer required */
    return RPCHelpMan{"getblockchaininfo",
        "Returns an object containing various state info regarding blockchain processing.\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "chain", "current network name (main, test, regtest)"},
                {RPCResult::Type::NUM, "blocks", "the height of the most-work fully-validated chain. The genesis block has height 0"},
                {RPCResult::Type::NUM, "headers", "the current number of headers we have validated"},
                {RPCResult::Type::STR, "bestblockhash", "the hash of the currently best block"},
                {RPCResult::Type::NUM, "difficulty", "the current difficulty"},
                {RPCResult::Type::NUM_TIME, "time", "The block time expressed in " + UNIX_EPOCH_TIME},
                {RPCResult::Type::NUM_TIME, "mediantime", "The median block time expressed in " + UNIX_EPOCH_TIME},
                {RPCResult::Type::NUM, "verificationprogress", "estimate of verification progress [0..1]"},
                {RPCResult::Type::BOOL, "initialblockdownload", "(debug information) estimate of whether this node is in Initial Block Download mode"},
                {RPCResult::Type::STR_HEX, "chainwork", "total amount of work in active chain, in hexadecimal"},
                {RPCResult::Type::NUM, "size_on_disk", "the estimated size of the block and undo files on disk"},
                {RPCResult::Type::BOOL, "pruned", "if the blocks are subject to pruning"},
                {RPCResult::Type::NUM, "pruneheight", /*optional=*/true, "height of the last block pruned, plus one (only present if pruning is enabled)"},
                {RPCResult::Type::BOOL, "automatic_pruning", /*optional=*/true, "whether automatic pruning is enabled (only present if pruning is enabled)"},
                {RPCResult::Type::NUM, "prune_target_size", /*optional=*/true, "the target size used by pruning (only present if automatic pruning is enabled)"},
                {RPCResult::Type::OBJ_DYN, "softforks", /*optional=*/true, "(DEPRECATED, returned only if config option -deprecatedrpc=softforks is passed) status of softforks",
                {
                    {RPCResult::Type::OBJ, "xxxx", "name of the softfork",
                        RPCHelpForDeployment
                    },
                }},
                {RPCResult::Type::STR, "warnings", "any network and blockchain warnings"},
            }},
        RPCExamples{
            HelpExampleCli("getblockchaininfo", "")
            + HelpExampleRpc("getblockchaininfo", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const node::NodeContext& node = EnsureAnyNodeContext(request.context);
    const ArgsManager& args{EnsureArgsman(node)};
    ChainstateManager& chainman = EnsureChainman(node);

    LOCK(cs_main);

    Chainstate& active_chainstate = chainman.ActiveChainstate();
    const CBlockIndex& tip{*CHECK_NONFATAL(active_chainstate.m_chain.Tip())};
    const int height{tip.nHeight};

    UniValue obj(UniValue::VOBJ);

    obj.pushKV("chain", chainman.GetParams().NetworkIDString());
    obj.pushKV("blocks", height);
    obj.pushKV("headers", chainman.m_best_header ? chainman.m_best_header->nHeight : -1);
    obj.pushKV("bestblockhash", tip.GetBlockHash().GetHex());
    obj.pushKV("difficulty", GetDifficulty(&tip));
    obj.pushKV("time", tip.GetBlockTime());
    obj.pushKV("mediantime", tip.GetMedianTimePast());
    obj.pushKV("verificationprogress", GuessVerificationProgress(chainman.GetParams().TxData(), &tip));
    obj.pushKV("initialblockdownload", active_chainstate.IsInitialBlockDownload());
    obj.pushKV("chainwork", tip.nChainWork.GetHex());
    obj.pushKV("size_on_disk", chainman.m_blockman.CalculateCurrentUsage());
    obj.pushKV("pruned", chainman.m_blockman.IsPruneMode());

    if (chainman.m_blockman.IsPruneMode()) {
        obj.pushKV("pruneheight", chainman.m_blockman.GetFirstStoredBlock(tip)->nHeight);

        // if 0, execution bypasses the whole if block.
        bool automatic_pruning{args.GetIntArg("-prune", 0) != 1};
        obj.pushKV("automatic_pruning", automatic_pruning);

        if (automatic_pruning) {
            obj.pushKV("prune_target_size", chainman.m_blockman.GetPruneTarget());
        }
    }

    if (IsDeprecatedRPCEnabled("softforks")) {
        const auto ehf_signals{active_chainstate.ChainHelper().GetSignalsStage(&tip)};
        obj.pushKV("softforks", DeploymentInfo(&tip, ehf_signals, chainman));
    }

    obj.pushKV("warnings", GetWarnings(false).original);

    return obj;
},
    };
}

/** Comparison function for sorting the getchaintips heads.  */
struct CompareBlocksByHeight {
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /* Make sure that unequal blocks with the same height do not compare
           equal. Use the pointers themselves to make a distinction. */

        if (a->nHeight != b->nHeight)
            return (a->nHeight > b->nHeight);

        return a < b;
    }
};

static RPCHelpMan getchaintips()
{
    return RPCHelpMan{"getchaintips",
                "Return information about all known tips in the block tree,"
                " including the main chain as well as orphaned branches.\n",
                {
                    {"count", RPCArg::Type::NUM, RPCArg::Default{INT_MAX}, "only show this much of latest tips"},
                    {"branchlen", RPCArg::Type::NUM, RPCArg::Default{-1}, "only show tips that have equal or greater length of branch"},
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {{RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::NUM, "height", "height of the chain tip"},
                            {RPCResult::Type::STR_HEX, "hash", "block hash of the tip"},
                            {RPCResult::Type::NUM, "difficulty", "The difficulty"},
                            {RPCResult::Type::STR_HEX, "chainwork", "Expected number of hashes required to produce the current chain (in hex)"},
                            {RPCResult::Type::NUM, "branchlen", "zero for main chain, otherwise length of branch connecting the tip to the main chain"},
                            {RPCResult::Type::STR_HEX, "forkpoint", "same as \"hash\" for the main chain"},
                            {RPCResult::Type::STR, "status", "status of the chain, \"active\" for the main chain\n"
            "Possible values for status:\n"
            "1.  \"invalid\"               This branch contains at least one invalid block\n"
            "2.  \"headers-only\"          Not all blocks for this branch are available, but the headers are valid\n"
            "3.  \"valid-headers\"         All blocks are available for this branch, but they were never fully validated\n"
            "4.  \"valid-fork\"            This branch is not part of the active chain, but is fully validated\n"
            "5.  \"active\"                This is the tip of the active main chain, which is certainly valid\n"
            "6.  \"conflicting\"           This block or one of its ancestors is conflicting with ChainLocks."},
                        }}}},
                RPCExamples{
                    HelpExampleCli("getchaintips", "")
            + HelpExampleRpc("getchaintips", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();

    /*
     * Idea: The set of chain tips is the active chain tip, plus orphan blocks which do not have another orphan building off of them.
     * Algorithm:
     *  - Make one pass through BlockIndex(), picking out the orphan blocks, and also storing a set of the orphan block's pprev pointers.
     *  - Iterate through the orphan blocks. If the block isn't pointed to by another orphan, it is a chain tip.
     *  - Add the active chain tip
     */
    std::set<const CBlockIndex*, CompareBlocksByHeight> setTips;
    std::set<const CBlockIndex*> setOrphans;
    std::set<const CBlockIndex*> setPrevs;

    for (const auto& [_, block_index] : chainman.BlockIndex()) {
        if (!active_chain.Contains(&block_index)) {
            setOrphans.insert(&block_index);
            setPrevs.insert(block_index.pprev);
        }
    }

    for (const auto& orphan : setOrphans) {
        if (setPrevs.erase(orphan) == 0) {
            setTips.insert(orphan);
        }
    }

    // Always report the currently active tip.
    setTips.insert(active_chain.Tip());

    int nCountMax{request.params[0].isNull() ? INT_MAX : request.params[0].getInt<int>()};
    const int nBranchMin{request.params[1].isNull() ? -1 : request.params[1].getInt<int>()};

    /* Construct the output array. */
    UniValue res(UniValue::VARR);

    for (const CBlockIndex* block : setTips)
    {
        const CBlockIndex* pindexFork = active_chain.FindFork(block);
        const int branchLen = block->nHeight - pindexFork->nHeight;

        if (branchLen < nBranchMin) continue;
        if (nCountMax-- < 1) break;

        UniValue obj(UniValue::VOBJ);
        obj.pushKV("height", block->nHeight);
        obj.pushKV("hash", block->phashBlock->GetHex());
        obj.pushKV("difficulty", GetDifficulty(block));
        obj.pushKV("chainwork", block->nChainWork.GetHex());
        obj.pushKV("branchlen", branchLen);
        obj.pushKV("forkpoint", pindexFork->phashBlock->GetHex());

        std::string status;

        if (active_chain.Contains(block)) {
            // This block is part of the currently active chain.
            status = "active";
        } else if (block->nStatus & BLOCK_FAILED_MASK) {
            // This block or one of its ancestors is invalid.
            status = "invalid";
        } else if (block->nStatus & BLOCK_CONFLICT_CHAINLOCK) {
            // This block or one of its ancestors is conflicting with ChainLocks.
            status = "conflicting";
        } else if (!block->HaveTxsDownloaded()) {
            // This block cannot be connected because full block data for it or one of its parents is missing.
            status = "headers-only";
        } else if (block->IsValid(BLOCK_VALID_SCRIPTS)) {
            // This block is fully validated, but no longer part of the active chain. It was probably the active block once, but was reorganized.
            status = "valid-fork";
        } else if (block->IsValid(BLOCK_VALID_TREE)) {
            // The headers for this block are valid, but it has not been validated. It was probably never part of the most-work chain.
            status = "valid-headers";
        } else {
            // No clue.
            status = "unknown";
        }

        obj.pushKV("status", status);
        res.push_back(obj);
    }

    return res;
},
    };
}

static RPCHelpMan getfeeinfo()
{
    return RPCHelpMan{"getfeeinfo",
        "\nReturns transaction-fee statistics over the requested number of recent blocks.\n",
        {
            {"blocks", RPCArg::Type::NUM, RPCArg::Optional::NO, "The number of recent blocks to include"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "Starting block", "First counted block height"},
                {RPCResult::Type::NUM, "Ending block", "Last counted block height"},
                {RPCResult::Type::NUM, "txcount", "Transaction count excluding coinbase and coinstake transactions"},
                {RPCResult::Type::NUM, "txcount_all", "Transaction count including coinbase and coinstake transactions"},
                {RPCResult::Type::NUM, "txbytes", "Total serialized size of ordinary transactions excluding historical Zerocoin transactions"},
                {RPCResult::Type::STR, "ttlfee", "Total fees excluding historical Zerocoin mint and spend transactions"},
                {RPCResult::Type::STR, "ttlfee_all", "Total fees including historical Zerocoin mint transactions but excluding historical Zerocoin spend transactions"},
                {RPCResult::Type::STR, "feeperkb", "Average fee per kilobyte excluding historical Zerocoin transactions"},
            }},
        RPCExamples{
            HelpExampleCli("getfeeinfo", "5")
            + HelpExampleRpc("getfeeinfo", "5")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman =
        EnsureAnyChainman(request.context);

    const int block_count{
        request.params[0].getInt<int>()};

    int best_height;
    {
        LOCK(cs_main);
        best_height =
            chainman.ActiveChain().Height();
    }

    if (block_count <= 0 ||
        block_count >= best_height) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            "Block count must be greater than zero and less than the active-chain height");
    }

    const int height_start{
        best_height - block_count + 1};

    const int height_end{
        best_height};

    const BlockIndexStats stats{
        CalculateBlockIndexStats(
            chainman,
            height_start,
            height_end,
            /*fee_only=*/true)};

    return BlockIndexStatsToJSON(
        stats,
        height_start,
        height_end,
        /*fee_only=*/true);
},
    };
}

static RPCHelpMan invalidateblock()
{
    return RPCHelpMan{"invalidateblock",
                "\nPermanently marks a block as invalid, as if it violated a consensus rule.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "the hash of the block to mark as invalid"},
                },
                RPCResult{RPCResult::Type::NONE, "", ""},
                RPCExamples{
                    HelpExampleCli("invalidateblock", "\"blockhash\"")
            + HelpExampleRpc("invalidateblock", "\"blockhash\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    uint256 hash(ParseHashV(request.params[0], "blockhash"));
    BlockValidationState state;
    CBlockIndex* pblockindex;
    ChainstateManager& chainman = EnsureAnyChainman(request.context);

    {
        LOCK(cs_main);
        pblockindex = chainman.m_blockman.LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    }

    Chainstate& active_chainstate = chainman.ActiveChainstate();
    active_chainstate.InvalidateBlock(state, pblockindex);

    if (state.IsValid()) {
        active_chainstate.ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.ToString());
    }

    return UniValue::VNULL;
},
    };
}

static RPCHelpMan reconsiderblock()
{
    return RPCHelpMan{"reconsiderblock",
                "\nRemoves invalidity status of a block, its ancestors and its descendants, reconsider them for activation.\n"
                "This can be used to undo the effects of invalidateblock.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "the hash of the block to reconsider"},
                    {"ignore_chainlocks", RPCArg::Type::BOOL, RPCArg::Default{false}, "if true, existing chainlocks will be ignored"},
                },
                RPCResult{RPCResult::Type::NONE, "", ""},
                RPCExamples{
                    HelpExampleCli("reconsiderblock", "\"blockhash\"")
            + HelpExampleRpc("reconsiderblock", "\"blockhash\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    Chainstate& active_chainstate = chainman.ActiveChainstate();
    uint256 hash(ParseHashV(request.params[0], "blockhash"));
    const bool ignore_chainlocks{request.params[1].isNull() ? false : request.params[1].get_bool()};

    {
        LOCK(cs_main);

        CBlockIndex* pblockindex =
            chainman.m_blockman.LookupBlockIndex(hash);

        if (!pblockindex) {
            throw JSONRPCError(
                RPC_INVALID_ADDRESS_OR_KEY,
                "Block not found");
        }

        active_chainstate.ResetBlockFailureFlags(
            pblockindex,
            ignore_chainlocks);
    }

    BlockValidationState state;
    active_chainstate.ActivateBestChain(state);

    if (!state.IsValid()) {
        throw JSONRPCError(
            RPC_DATABASE_ERROR,
            state.ToString());
    }

    return UniValue::VNULL;
},
    };
}

static RPCHelpMan getchaintxstats()
{
    return RPCHelpMan{"getchaintxstats",
                "\nCompute statistics about the total number and rate of transactions in the chain.\n",
                {
                    {"nblocks", RPCArg::Type::NUM, RPCArg::DefaultHint{"one month"}, "Size of the window in number of blocks"},
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::DefaultHint{"chain tip"}, "The hash of the block that ends the window."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM_TIME, "time", "The timestamp for the final block in the window, expressed in " + UNIX_EPOCH_TIME},
                        {RPCResult::Type::NUM, "txcount", "The total number of transactions in the chain up to that point"},
                        {RPCResult::Type::STR_HEX, "window_final_block_hash", "The hash of the final block in the window"},
                        {RPCResult::Type::NUM, "window_final_block_height", "The height of the final block in the window."},
                        {RPCResult::Type::NUM, "window_block_count", "Size of the window in number of blocks"},
                        {RPCResult::Type::NUM, "window_tx_count", /*optional=*/true, "The number of transactions in the window. Only returned if \"window_block_count\" is > 0"},
                        {RPCResult::Type::NUM, "window_interval", /*optional=*/true, "The elapsed time in the window in seconds. Only returned if \"window_block_count\" is > 0"},
                        {RPCResult::Type::NUM, "txrate", /*optional=*/true, "The average rate of transactions per second in the window. Only returned if \"window_interval\" is > 0"},
                    }},
                RPCExamples{
                    HelpExampleCli("getchaintxstats", "")
            + HelpExampleRpc("getchaintxstats", "2016")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    const CBlockIndex* pindex;

    // Preserve SCH's historical consensus target spacing.
    int blockcount = 30 * 24 * 60 * 60 / chainman.GetParams().GetConsensus().nTargetSpacing;

    if (request.params[1].isNull()) {
        LOCK(cs_main);
        pindex = chainman.ActiveChain().Tip();
    } else {
        uint256 hash(ParseHashV(request.params[1], "blockhash"));
        LOCK(cs_main);

        pindex = chainman.m_blockman.LookupBlockIndex(hash);

        if (!pindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }

        if (!chainman.ActiveChain().Contains(pindex)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Block is not in main chain");
        }
    }

    CHECK_NONFATAL(pindex != nullptr);

    if (request.params[0].isNull()) {
        blockcount = std::max(0, std::min(blockcount, pindex->nHeight - 1));
    } else {
        blockcount = request.params[0].getInt<int>();

        if (blockcount < 0 || (blockcount > 0 && blockcount >= pindex->nHeight)) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Invalid block count: should be between 0 and the block's height - 1");
        }
    }

    const CBlockIndex& past_block{
        *CHECK_NONFATAL(
            pindex->GetAncestor(
                pindex->nHeight - blockcount))};

    const int64_t nTimeDiff{
        pindex->GetMedianTimePast() -
        past_block.GetMedianTimePast()};

    const int nTxDiff =
        pindex->nChainTx -
        past_block.nChainTx;

    UniValue ret(UniValue::VOBJ);

    ret.pushKV("time", (int64_t)pindex->nTime);
    ret.pushKV("txcount", (int64_t)pindex->nChainTx);
    ret.pushKV("window_final_block_hash", pindex->GetBlockHash().GetHex());
    ret.pushKV("window_final_block_height", pindex->nHeight);
    ret.pushKV("window_block_count", blockcount);

    if (blockcount > 0) {
        ret.pushKV("window_tx_count", nTxDiff);
        ret.pushKV("window_interval", nTimeDiff);

        if (nTimeDiff > 0) {
            ret.pushKV("txrate", ((double)nTxDiff) / nTimeDiff);
        }
    }

    return ret;
},
    };
}

static void ValidateRange(
    const UniValue& params,
    ChainstateManager& chainman,
    int& height_start,
    int& height_end,
    int minimum_start_height = 1)
{
    if (params.size() < 2) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            "Not enough parameters in ValidateRange");
    }

    int best_height;
    {
        LOCK(cs_main);
        best_height = chainman.ActiveChain().Height();
    }

    height_start = params[0].getInt<int>();

    if (height_start < 0) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            strprintf(
                "Invalid starting block (%d). Height must not be negative.",
                height_start));
    }

    if (height_start > best_height) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            strprintf(
                "Invalid starting block (%d). Out of range.",
                height_start));
    }

    const int range = params[1].getInt<int>();

    if (range < 1) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            "Invalid block range. Must be strictly positive.");
    }

    const int64_t calculated_end =
        static_cast<int64_t>(height_start) +
        static_cast<int64_t>(range) -
        1;

    if (calculated_end > best_height) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            strprintf(
                "Invalid ending block (%d). Out of range.",
                calculated_end));
    }

    height_end = static_cast<int>(calculated_end);

    if (height_start < minimum_start_height &&
        height_end >= minimum_start_height) {
        height_start = minimum_start_height;
    }

    if (height_end < minimum_start_height) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            strprintf(
                "Requested range ends below minimum block height %d.",
                minimum_start_height));
    }
}

struct BlockIndexStats
{
    CAmount fees{0};
    CAmount fees_all{0};
    int64_t bytes{0};
    int64_t transaction_count{0};
    int64_t transaction_count_all{0};
    std::map<libzerocoin::CoinDenomination, int64_t> spend_count;

    BlockIndexStats()
    {
        for (const auto denomination :
             libzerocoin::zerocoinDenomList) {
            spend_count.emplace(denomination, 0);
        }
    }
};

static BlockIndexStats CalculateBlockIndexStats(
    ChainstateManager& chainman,
    int height_start,
    int height_end,
    bool fee_only)
{
    std::vector<const CBlockIndex*> block_indexes;
    block_indexes.reserve(height_end - height_start + 1);

    {
        LOCK(cs_main);
        const CChain& active_chain = chainman.ActiveChain();

        for (int height = height_start;
             height <= height_end;
             ++height) {
            const CBlockIndex* block_index =
                active_chain[height];

            if (!block_index) {
                throw JSONRPCError(
                    RPC_DATABASE_ERROR,
                    strprintf(
                        "Failed to locate active-chain block at height %d",
                        height));
            }

            block_indexes.push_back(block_index);
        }
    }

    BlockIndexStats stats;

    for (const CBlockIndex* block_index :
         block_indexes) {
        CBlock block;

        if (!node::ReadBlockFromDisk(
                block,
                block_index,
                chainman.GetConsensus())) {
            throw JSONRPCError(
                RPC_DATABASE_ERROR,
                strprintf(
                    "Failed to read block at height %d from disk",
                    block_index->nHeight));
        }

        CBlockUndo block_undo;

        if (!node::UndoReadFromDisk(
                block_undo,
                block_index)) {
            throw JSONRPCError(
                RPC_DATABASE_ERROR,
                strprintf(
                    "Failed to read undo data at height %d",
                    block_index->nHeight));
        }

        if (block.vtx.empty() ||
            block_undo.vtxundo.size() !=
                block.vtx.size() - 1) {
            throw JSONRPCError(
                RPC_DATABASE_ERROR,
                strprintf(
                    "Block and undo data are inconsistent at height %d",
                    block_index->nHeight));
        }

        const int64_t block_transaction_count{
            static_cast<int64_t>(block.vtx.size())};

        stats.transaction_count_all +=
            block_transaction_count;

        stats.transaction_count +=
            block.IsProofOfStake()
                ? block_transaction_count - 2
                : block_transaction_count - 1;

        for (size_t i = 1;
             i < block.vtx.size();
             ++i) {
            const CTransaction& tx{
                *block.vtx[i]};

            if (tx.IsCoinStake() &&
                !tx.HasZerocoinSpendInputs()) {
                continue;
            }

            if (tx.HasZerocoinSpendInputs()) {
                if (!fee_only) {
                    for (const CTxIn& input :
                         tx.vin) {
                        if (!input.IsZerocoinSpend()) {
                            continue;
                        }

                        const auto denomination{
                            libzerocoin::
                                IntToZerocoinDenomination(
                                    input.nSequence)};

                        if (denomination !=
                            libzerocoin::ZQ_ERROR) {
                            ++stats.spend_count[
                                denomination];
                        }
                    }
                }

                // Historical Zerocoin spends consume no ordinary
                // UTXOs and are excluded from fee accounting.
                continue;
            }

            const CTxUndo& tx_undo{
                block_undo.vtxundo.at(i - 1)};

            if (tx_undo.vprevout.size() !=
                tx.vin.size()) {
                throw JSONRPCError(
                    RPC_DATABASE_ERROR,
                    strprintf(
                        "Transaction and undo data are inconsistent at height %d",
                        block_index->nHeight));
            }

            CAmount value_in{0};

            for (const Coin& coin :
                 tx_undo.vprevout) {
                value_in += coin.out.nValue;
            }

            const CAmount fee{
                value_in - tx.GetValueOut()};

            stats.fees_all += fee;

            if (!tx.HasZerocoinMintOutputs()) {
                stats.fees += fee;
                stats.bytes +=
                    ::GetSerializeSize(
                        tx,
                        SER_NETWORK,
                        PROTOCOL_VERSION);
            }
        }
    }

    return stats;
}

static UniValue BlockIndexStatsToJSON(
    const BlockIndexStats& stats,
    int height_start,
    int height_end,
    bool fee_only)
{
    UniValue result(UniValue::VOBJ);

    result.pushKV(
        "Starting block",
        height_start);

    result.pushKV(
        "Ending block",
        height_end);

    result.pushKV(
        "txcount",
        stats.transaction_count);

    result.pushKV(
        "txcount_all",
        stats.transaction_count_all);

    if (!fee_only) {
        UniValue spend_count(UniValue::VOBJ);

        for (const auto denomination :
             libzerocoin::zerocoinDenomList) {
            spend_count.pushKV(
                strprintf(
                    "denom_%d",
                    libzerocoin::
                        ZerocoinDenominationToInt(
                            denomination)),
                stats.spend_count.at(
                    denomination));
        }

        result.pushKV(
            "spendcount",
            spend_count);
    }

    const CFeeRate fee_rate{
        stats.fees,
        stats.bytes};

    result.pushKV(
        "txbytes",
        stats.bytes);

    result.pushKV(
        "ttlfee",
        FormatMoney(stats.fees));

    result.pushKV(
        "ttlfee_all",
        FormatMoney(stats.fees_all));

    result.pushKV(
        "feeperkb",
        FormatMoney(
            fee_rate.GetFeePerK()));

    return result;
}

static RPCHelpMan getblockindexstats()
{
    return RPCHelpMan{"getblockindexstats",
        "\nReturns aggregated block-index statistics for the requested active-chain block range.\n",
        {
            {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "Block height where the scan starts"},
            {"range", RPCArg::Type::NUM, RPCArg::Optional::NO, "Number of consecutive active-chain blocks to include"},
            {"fee_only", RPCArg::Type::BOOL, RPCArg::Default{false}, "If true, omit historical Zerocoin denomination spend counts"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "Starting block", "First counted block height"},
                {RPCResult::Type::NUM, "Ending block", "Last counted block height"},
                {RPCResult::Type::NUM, "txcount", "Transaction count excluding coinbase and coinstake transactions"},
                {RPCResult::Type::NUM, "txcount_all", "Transaction count including coinbase and coinstake transactions"},
                {RPCResult::Type::OBJ_DYN, "spendcount", /*optional=*/true, "Historical Zerocoin spend counts by denomination",
                {
                    {RPCResult::Type::NUM, "denom_x", "Number of historical spends of this denomination"},
                }},
                {RPCResult::Type::NUM, "txbytes", "Total serialized size of ordinary transactions excluding historical Zerocoin mint and spend transactions"},
                {RPCResult::Type::STR, "ttlfee", "Total fees excluding historical Zerocoin mint and spend transactions"},
                {RPCResult::Type::STR, "ttlfee_all", "Total fees including historical Zerocoin mint transactions but excluding historical Zerocoin spend transactions"},
                {RPCResult::Type::STR, "feeperkb", "Average fee per kilobyte excluding historical Zerocoin transactions"},
            }},
        RPCExamples{
            HelpExampleCli("getblockindexstats", "1200000 1000")
            + HelpExampleRpc("getblockindexstats", "1200000, 1000")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman =
        EnsureAnyChainman(request.context);

    int height_start;
    int height_end;

    ValidateRange(
        request.params,
        chainman,
        height_start,
        height_end);

    const bool fee_only{
        request.params[2].isNull()
            ? false
            : request.params[2].get_bool()};

    const BlockIndexStats stats{
        CalculateBlockIndexStats(
            chainman,
            height_start,
            height_end,
            fee_only)};

    return BlockIndexStatsToJSON(
        stats,
        height_start,
        height_end,
        fee_only);
},
    };
}

static RPCHelpMan gettransactionversionstats()
{
    return RPCHelpMan{"gettransactionversionstats",
        "\nScans transactions in active-chain blocks and reports every distinct signed 32-bit transaction version.\n",
        {
            {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "Block height where the scan starts"},
            {"range", RPCArg::Type::NUM, RPCArg::Optional::NO, "Number of consecutive active-chain blocks to scan"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "start_height", "First scanned block height"},
                {RPCResult::Type::NUM, "end_height", "Last scanned block height"},
                {RPCResult::Type::NUM, "blocks_scanned", "Number of blocks scanned"},
                {RPCResult::Type::NUM, "transactions_scanned", "Number of transactions scanned"},
                {RPCResult::Type::BOOL, "all_upper_16_bits_zero", "Whether every scanned transaction version has zero in its upper 16 bits"},
                {RPCResult::Type::ARR, "versions", "Statistics for each distinct signed transaction version",
                {
                    {RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "version", "Signed 32-bit transaction version"},
                        {RPCResult::Type::STR_HEX, "version_hex", "Transaction version formatted as eight hexadecimal digits"},
                        {RPCResult::Type::NUM, "lower_16_bits", "Lower 16 bits of the transaction version"},
                        {RPCResult::Type::NUM, "upper_16_bits", "Upper 16 bits of the transaction version"},
                        {RPCResult::Type::BOOL, "upper_16_bits_nonzero", "Whether the upper 16 bits are nonzero"},
                        {RPCResult::Type::NUM, "transaction_count", "Number of transactions using this version"},
                        {RPCResult::Type::NUM, "coinbase_count", "Number of coinbase transactions using this version"},
                        {RPCResult::Type::NUM, "coinstake_count", "Number of coinstake transactions using this version"},
                        {RPCResult::Type::NUM, "first_height", "First scanned height containing this version"},
                        {RPCResult::Type::NUM, "last_height", "Last scanned height containing this version"},
                        {RPCResult::Type::STR_HEX, "example_txid", "Example transaction identifier using this version"},
                    }},
                }},
            }},
        RPCExamples{
            HelpExampleCli("gettransactionversionstats", "0 1001")
            + HelpExampleRpc("gettransactionversionstats", "0, 1001")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const node::NodeContext& node =
        EnsureAnyNodeContext(request.context);

    ChainstateManager& chainman =
        EnsureChainman(node);

    int height_start;
    int height_end;

    ValidateRange(
        request.params,
        chainman,
        height_start,
        height_end,
        /*minimum_start_height=*/0);

    std::vector<const CBlockIndex*> block_indexes;
    block_indexes.reserve(
        height_end - height_start + 1);

    {
        LOCK(cs_main);

        const CChain& active_chain =
            chainman.ActiveChain();

        for (int height = height_start;
             height <= height_end;
             ++height) {
            const CBlockIndex* block_index =
                active_chain[height];

            if (!block_index) {
                throw JSONRPCError(
                    RPC_DATABASE_ERROR,
                    strprintf(
                        "Failed to locate active-chain block at height %d",
                        height));
            }

            block_indexes.push_back(block_index);
        }
    }

    struct TransactionVersionStats
    {
        int64_t transaction_count{0};
        int64_t coinbase_count{0};
        int64_t coinstake_count{0};
        int first_height{-1};
        int last_height{-1};
        uint256 example_txid{};
    };

    std::map<int32_t, TransactionVersionStats>
        version_stats;

    int64_t blocks_scanned{0};
    int64_t transactions_scanned{0};
    bool all_upper_16_bits_zero{true};

    for (const CBlockIndex* block_index :
         block_indexes) {
        node.rpc_interruption_point();

        CBlock block;

        if (!node::ReadBlockFromDisk(
                block,
                block_index,
                chainman.GetConsensus())) {
            throw JSONRPCError(
                RPC_DATABASE_ERROR,
                strprintf(
                    "Failed to read block at height %d from disk",
                    block_index->nHeight));
        }

        ++blocks_scanned;

        for (const CTransactionRef& tx_ref :
             block.vtx) {
            const CTransaction& tx{*tx_ref};

            ++transactions_scanned;

            TransactionVersionStats& stats{
                version_stats[tx.nVersion]};

            if (stats.transaction_count == 0) {
                stats.first_height =
                    block_index->nHeight;

                stats.example_txid =
                    tx.GetHash();
            }

            ++stats.transaction_count;

            stats.last_height =
                block_index->nHeight;

            if (tx.IsCoinBase()) {
                ++stats.coinbase_count;
            }

            if (tx.IsCoinStake()) {
                ++stats.coinstake_count;
            }

            const uint32_t raw_version{
                static_cast<uint32_t>(
                    tx.nVersion)};

            const uint32_t upper_16_bits{
                (raw_version >> 16) &
                0xffffU};

            if (upper_16_bits != 0) {
                all_upper_16_bits_zero = false;
            }
        }
    }

    UniValue versions(UniValue::VARR);

    for (const auto& [transaction_version, stats] :
         version_stats) {
        const uint32_t raw_version{
            static_cast<uint32_t>(
                transaction_version)};

        const uint32_t lower_16_bits{
            raw_version & 0xffffU};

        const uint32_t upper_16_bits{
            (raw_version >> 16) &
            0xffffU};

        UniValue version_entry(UniValue::VOBJ);

        version_entry.pushKV(
            "version",
            transaction_version);

        version_entry.pushKV(
            "version_hex",
            strprintf(
                "%08x",
                raw_version));

        version_entry.pushKV(
            "lower_16_bits",
            static_cast<int64_t>(
                lower_16_bits));

        version_entry.pushKV(
            "upper_16_bits",
            static_cast<int64_t>(
                upper_16_bits));

        version_entry.pushKV(
            "upper_16_bits_nonzero",
            upper_16_bits != 0);

        version_entry.pushKV(
            "transaction_count",
            stats.transaction_count);

        version_entry.pushKV(
            "coinbase_count",
            stats.coinbase_count);

        version_entry.pushKV(
            "coinstake_count",
            stats.coinstake_count);

        version_entry.pushKV(
            "first_height",
            stats.first_height);

        version_entry.pushKV(
            "last_height",
            stats.last_height);

        version_entry.pushKV(
            "example_txid",
            stats.example_txid.GetHex());

        versions.push_back(version_entry);
    }

    UniValue result(UniValue::VOBJ);

    result.pushKV(
        "start_height",
        height_start);

    result.pushKV(
        "end_height",
        height_end);

    result.pushKV(
        "blocks_scanned",
        blocks_scanned);

    result.pushKV(
        "transactions_scanned",
        transactions_scanned);

    result.pushKV(
        "all_upper_16_bits_zero",
        all_upper_16_bits_zero);

    result.pushKV(
        "versions",
        versions);

    return result;
},
    };
}

void RegisterBlockchainRPCCommands(CRPCTable& table_rpc)
{
    static const CRPCCommand commands[]{
        {"blockchain", &getblockchaininfo},
        {"blockchain", &getchaintxstats},
        {"blockchain", &getbestblockhash},
        {"blockchain", &getblockcount},
        {"blockchain", &getblock},
        {"blockchain", &getblockhash},
        {"blockchain", &getblockheader},
        {"blockchain", &getchaintips},
        {"blockchain", &getdifficulty},
        {"blockchain", &gettxout},
        {"blockchain", &gettxoutsetinfo},
        {"blockchain", &verifychain},
        {"blockchain", &getfeeinfo},
        {"blockchain", &getblockindexstats},
        {"blockchain", &gettransactionversionstats},
        {"hidden", &invalidateblock},
        {"hidden", &reconsiderblock},
        {"hidden", &waitfornewblock},
        {"hidden", &waitforblock},
        {"hidden", &waitforblockheight},
    };

    for (const auto& command : commands) {
        table_rpc.appendCommand(command.name, &command);
    }
}