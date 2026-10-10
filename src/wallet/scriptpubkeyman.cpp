// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2020 The PIVX developers
// Copyright (c) 2021 The DECENOMY Core Developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/scriptpubkeyman.h"

#include "script/standard.h"
#include "util.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>

static int64_t GetOldestKeyTimeInPool(const std::set<int64_t>& keypool_set, CWalletDB& batch)
{
    if (keypool_set.empty()) {
        return GetTime();
    }

    CKeyPool keypool;
    const int64_t index = *keypool_set.begin();
    if (!batch.ReadPool(index, keypool)) {
        throw std::runtime_error(std::string(__func__) + ": read oldest key in keypool failed");
    }
    assert(keypool.vchPubKey.IsValid());
    return keypool.nTime;
}

void ScriptPubKeyMan::LoadKeyPool(int64_t index, const CKeyPool& keypool)
{
    AssertLockHeld(wallet->cs_wallet);

    if (keypool.m_pre_split) {
        set_pre_split_keypool.insert(index);
    } else if (keypool.fInternal) {
        setInternalKeyPool.insert(index);
    } else {
        setExternalKeyPool.insert(index);
    }

    m_max_keypool_index = std::max(m_max_keypool_index, index);
    m_pool_key_to_index[keypool.vchPubKey.GetID()] = index;

    const CKeyID keyid = keypool.vchPubKey.GetID();
    if (wallet->mapKeyMetadata.count(keyid) == 0) {
        wallet->mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);
    }
}

bool ScriptPubKeyMan::NewKeyPool()
{
    LOCK(wallet->cs_wallet);

    CWalletDB batch(wallet->strWalletFile);

    for (const int64_t index : setInternalKeyPool) {
        batch.ErasePool(index);
    }
    setInternalKeyPool.clear();

    for (const int64_t index : setExternalKeyPool) {
        batch.ErasePool(index);
    }
    setExternalKeyPool.clear();

    for (const int64_t index : set_pre_split_keypool) {
        batch.ErasePool(index);
    }
    set_pre_split_keypool.clear();

    m_pool_key_to_index.clear();
    m_index_to_reserved_key.clear();

    if (!TopUp()) {
        return false;
    }

    LogPrintf("ScriptPubKeyMan::NewKeyPool rewrote keypool\n");
    return true;
}

bool ScriptPubKeyMan::TopUp(unsigned int size)
{
    LOCK(wallet->cs_wallet);

    if (wallet->IsLocked()) {
        return false;
    }

    const unsigned int target_size = size > 0 ?
            size :
            std::max(GetArg("-keypool", DEFAULT_KEYPOOL_SIZE), int64_t{0});

    int64_t missing_external = std::max(
            std::max(static_cast<int64_t>(target_size), int64_t{1}) -
                    static_cast<int64_t>(setExternalKeyPool.size()),
            int64_t{0});

    int64_t missing_internal = std::max(
            std::max(static_cast<int64_t>(target_size), int64_t{1}) -
                    static_cast<int64_t>(setInternalKeyPool.size()),
            int64_t{0});

    if (!wallet->IsHDEnabled()) {
        missing_internal = 0;
    }

    CWalletDB batch(wallet->strWalletFile);
    GeneratePool(batch, missing_external, false);
    GeneratePool(batch, missing_internal, true);

    if (missing_external + missing_internal > 0) {
        LogPrintf("keypool added %d keys (%d internal), size=%u (%u internal)\n",
                  missing_external + missing_internal,
                  missing_internal,
                  setInternalKeyPool.size() + setExternalKeyPool.size() + set_pre_split_keypool.size(),
                  setInternalKeyPool.size());
    }

    return true;
}

void ScriptPubKeyMan::GeneratePool(CWalletDB& batch, int64_t target_size, bool internal)
{
    for (int64_t i = target_size; i > 0; --i) {
        const CPubKey pubkey = wallet->GenerateNewKey(0, internal);
        AddKeypoolPubkeyWithDB(pubkey, internal, batch);
    }
}

void ScriptPubKeyMan::AddKeypoolPubkeyWithDB(const CPubKey& pubkey, bool internal, CWalletDB& batch)
{
    AssertLockHeld(wallet->cs_wallet);
    assert(m_max_keypool_index < std::numeric_limits<int64_t>::max());

    const int64_t index = ++m_max_keypool_index;
    if (!batch.WritePool(index, CKeyPool(pubkey, internal))) {
        throw std::runtime_error(std::string(__func__) + ": writing generated pubkey failed");
    }

    if (internal) {
        setInternalKeyPool.insert(index);
    } else {
        setExternalKeyPool.insert(index);
    }

    m_pool_key_to_index[pubkey.GetID()] = index;
}

void ScriptPubKeyMan::MarkPreSplitKeys()
{
    LOCK(wallet->cs_wallet);

    CWalletDB batch(wallet->strWalletFile);
    for (auto it = setExternalKeyPool.begin(); it != setExternalKeyPool.end();) {
        const int64_t index = *it;
        CKeyPool keypool;
        if (!batch.ReadPool(index, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": read keypool entry failed");
        }

        keypool.m_pre_split = true;
        if (!batch.WritePool(index, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": writing modified keypool entry failed");
        }

        set_pre_split_keypool.insert(index);
        it = setExternalKeyPool.erase(it);
    }
}

size_t ScriptPubKeyMan::KeypoolCountExternalKeys() const
{
    AssertLockHeld(wallet->cs_wallet);
    return setExternalKeyPool.size() + set_pre_split_keypool.size();
}

size_t ScriptPubKeyMan::KeypoolCountInternalKeys() const
{
    AssertLockHeld(wallet->cs_wallet);
    return setInternalKeyPool.size();
}

unsigned int ScriptPubKeyMan::GetKeyPoolSize() const
{
    AssertLockHeld(wallet->cs_wallet);
    return setInternalKeyPool.size() + setExternalKeyPool.size() + set_pre_split_keypool.size();
}

int64_t ScriptPubKeyMan::GetOldestKeyPoolTime()
{
    LOCK(wallet->cs_wallet);

    CWalletDB batch(wallet->strWalletFile);
    int64_t oldest_key = GetOldestKeyTimeInPool(setExternalKeyPool, batch);

    if (wallet->IsHDEnabled()) {
        oldest_key = std::max(GetOldestKeyTimeInPool(setInternalKeyPool, batch), oldest_key);
        if (!set_pre_split_keypool.empty()) {
            oldest_key = std::max(GetOldestKeyTimeInPool(set_pre_split_keypool, batch), oldest_key);
        }
    }

    return oldest_key;
}

bool ScriptPubKeyMan::GetKeyFromPool(CPubKey& result, bool internal)
{
    CKeyPool keypool;
    int64_t index = -1;

    if (!ReserveKeyFromKeyPool(index, keypool, internal)) {
        if (wallet->IsLocked()) {
            LogPrintf("%s: Wallet locked, cannot get address\n", __func__);
            return false;
        }

        result = wallet->GenerateNewKey(0, internal);
        return true;
    }

    KeepDestination(index);
    result = keypool.vchPubKey;
    return true;
}

bool ScriptPubKeyMan::GetReservedKey(bool internal, int64_t& index, CKeyPool& keypool)
{
    if (!ReserveKeyFromKeyPool(index, keypool, internal)) {
        return error("%s: Cannot reserve key from pool", __func__);
    }
    return true;
}

bool ScriptPubKeyMan::ReserveKeyFromKeyPool(int64_t& index, CKeyPool& keypool, bool internal)
{
    index = -1;
    keypool.vchPubKey = CPubKey();

    LOCK(wallet->cs_wallet);

    if (!wallet->IsLocked()) {
        TopUp();
    }

    // Preserve SCH behavior:
    // Non-HD wallets only use the external pool.
    internal = internal && wallet->IsHDEnabled();

    const bool use_pre_split = !set_pre_split_keypool.empty();
    std::set<int64_t>& keypool_set = use_pre_split ?
            set_pre_split_keypool :
            (internal ? setInternalKeyPool : setExternalKeyPool);

    if (keypool_set.empty()) {
        return false;
    }

    CWalletDB batch(wallet->strWalletFile);

    const auto it = keypool_set.begin();
    index = *it;
    keypool_set.erase(it);

    if (!batch.ReadPool(index, keypool)) {
        throw std::runtime_error(std::string(__func__) + ": read failed");
    }

    CPubKey pubkey;
    if (!wallet->GetPubKey(keypool.vchPubKey.GetID(), pubkey)) {
        throw std::runtime_error(std::string(__func__) + ": unknown key in keypool");
    }

    if (!use_pre_split && keypool.fInternal != internal) {
        throw std::runtime_error(std::string(__func__) + ": keypool internal entry misclassified");
    }

    if (!keypool.vchPubKey.IsValid()) {
        throw std::runtime_error(std::string(__func__) + ": keypool entry invalid");
    }

    assert(m_index_to_reserved_key.count(index) == 0);
    m_index_to_reserved_key[index] = keypool.vchPubKey.GetID();
    m_pool_key_to_index.erase(keypool.vchPubKey.GetID());

    LogPrintf("%s: keypool reserve %d\n", __func__, index);
    return true;
}

void ScriptPubKeyMan::KeepDestination(int64_t index)
{
    CWalletDB batch(wallet->strWalletFile);
    batch.ErasePool(index);

    const auto it = m_index_to_reserved_key.find(index);
    assert(it != m_index_to_reserved_key.end());

    CPubKey pubkey;
    const bool have_pubkey = wallet->GetPubKey(it->second, pubkey);
    assert(have_pubkey);

    m_index_to_reserved_key.erase(it);
    LogPrintf("keypool keep %d\n", index);
}

void ScriptPubKeyMan::ReturnDestination(int64_t index, bool internal)
{
    LOCK(wallet->cs_wallet);

    if (internal) {
        setInternalKeyPool.insert(index);
    } else if (!set_pre_split_keypool.empty()) {
        set_pre_split_keypool.insert(index);
    } else {
        setExternalKeyPool.insert(index);
    }

    const auto it = m_index_to_reserved_key.find(index);
    assert(it != m_index_to_reserved_key.end());
    m_pool_key_to_index[it->second] = index;
    m_index_to_reserved_key.erase(it);

    LogPrintf("%s: keypool return %d\n", __func__, index);
}

void ScriptPubKeyMan::MarkReserveKeysAsUsed(int64_t keypool_id)
{
    AssertLockHeld(wallet->cs_wallet);

    std::set<int64_t>* keypool_set = nullptr;
    if (setInternalKeyPool.count(keypool_id) != 0) {
        keypool_set = &setInternalKeyPool;
    } else if (setExternalKeyPool.count(keypool_id) != 0) {
        keypool_set = &setExternalKeyPool;
    } else {
        assert(set_pre_split_keypool.count(keypool_id) != 0);
        keypool_set = &set_pre_split_keypool;
    }

    CWalletDB batch(wallet->strWalletFile);
    auto it = keypool_set->begin();
    while (it != keypool_set->end()) {
        const int64_t index = *it;
        if (index > keypool_id) {
            break;
        }

        CKeyPool keypool;
        if (batch.ReadPool(index, keypool)) {
            m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
        }

        batch.ErasePool(index);
        LogPrintf("keypool index %d removed\n", index);
        it = keypool_set->erase(it);
    }
}

void ScriptPubKeyMan::MarkUnusedAddresses(const CScript& script)
{
    AssertLockHeld(wallet->cs_wallet);

    for (const CKeyID& keyid : wallet->GetAffectedKeys(script)) {
        const auto it = m_pool_key_to_index.find(keyid);
        if (it == m_pool_key_to_index.end()) {
            continue;
        }

        LogPrintf("%s: Detected a used keypool key, marking earlier keypool keys as used\n", __func__);
        MarkReserveKeysAsUsed(it->second);

        if (!TopUp()) {
            LogPrintf("%s: Topping up keypool failed (locked wallet)\n", __func__);
        }
    }
}