// Copyright (c) 2026 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_LOGGING_CATEGORIES_H
#define SCHILLINGCOIN_LOGGING_CATEGORIES_H

#include <cstdint>

namespace BCLog {

using CategoryMask = uint64_t;

enum LogFlags : CategoryMask {
    NONE = CategoryMask{0},
    NET = (CategoryMask{1} << 0),
    TOR = (CategoryMask{1} << 1),
    MEMPOOL = (CategoryMask{1} << 2),
    HTTP = (CategoryMask{1} << 3),
    BENCH = (CategoryMask{1} << 4),
    ZMQ = (CategoryMask{1} << 5),
    WALLETDB = (CategoryMask{1} << 6),
    RPC = (CategoryMask{1} << 7),
    ESTIMATEFEE = (CategoryMask{1} << 8),
    ADDRMAN = (CategoryMask{1} << 9),
    SELECTCOINS = (CategoryMask{1} << 10),
    REINDEX = (CategoryMask{1} << 11),
    CMPCTBLOCK = (CategoryMask{1} << 12),
    RAND = (CategoryMask{1} << 13),
    PRUNE = (CategoryMask{1} << 14),
    PROXY = (CategoryMask{1} << 15),
    MEMPOOLREJ = (CategoryMask{1} << 16),
    COINDB = (CategoryMask{1} << 17),
    QT = (CategoryMask{1} << 18),
    LEVELDB = (CategoryMask{1} << 19),
    VALIDATION = (CategoryMask{1} << 20),
    I2P = (CategoryMask{1} << 21),
    IPC = (CategoryMask{1} << 22),
#ifdef DEBUG_LOCKCONTENTION
    LOCK = (CategoryMask{1} << 23),
#endif
    BLOCKSTORAGE = (CategoryMask{1} << 24),
    TXRECONCILIATION = (CategoryMask{1} << 25),
    SCAN = (CategoryMask{1} << 26),
    TXPACKAGES = (CategoryMask{1} << 27),
    KERNEL = (CategoryMask{1} << 28),
    PRIVBROADCAST = (CategoryMask{1} << 29),

    CHAINLOCKS = (CategoryMask{1} << 30),
    INSTANTSEND = (CategoryMask{1} << 31),
    LLMQ = (CategoryMask{1} << 32),
    LLMQ_DKG = (CategoryMask{1} << 33),
    LLMQ_SIGS = (CategoryMask{1} << 34),
    MNPAYMENTS = (CategoryMask{1} << 35),
    MNSYNC = (CategoryMask{1} << 36),
    NETCONN = (CategoryMask{1} << 37),
    EHF = (CategoryMask{1} << 38),
    HDWALLET = (CategoryMask{1} << 39),
    POS = (CategoryMask{1} << 40),
    STAKING = (CategoryMask{1} << 41),
    DB = (CategoryMask{1} << 42),

    // Transitional categories pending retirement of excluded subsystems.
    GOBJECT = (CategoryMask{1} << 43),
    COINJOIN = (CategoryMask{1} << 44),
    SPORK = (CategoryMask{1} << 45),
    CREDITPOOL = (CategoryMask{1} << 46),

    NET_NETCONN = NET | NETCONN,
    DASH = CHAINLOCKS | INSTANTSEND | LLMQ | LLMQ_DKG | LLMQ_SIGS |
           MNPAYMENTS | MNSYNC | NETCONN | EHF,

    ALL = ~NONE,
};

} // namespace BCLog

#endif // SCHILLINGCOIN_LOGGING_CATEGORIES_H