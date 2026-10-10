// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2020 The PIVX developers
// Copyright (c) 2021 The DECENOMY Core Developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_SCRIPTPUBKEYMAN_H
#define SCHILLINGCOIN_WALLET_SCRIPTPUBKEYMAN_H

#include "pubkey.h"
#include "script/script.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>

class CKeyPool;
class CWallet;
class CWalletDB;

//! Default for -keypool.
static const uint32_t DEFAULT_KEYPOOL_SIZE = 1000;

/**
 * Manages the wallet keypool and reserved destinations.
 *
 * SCH retains its existing similar-PAC HD wallet engine in CWallet.
 * CHDChain, CHDAccount, CHDPubKey, mnemonic handling, encrypted HD-chain
 * persistence, and child-key derivation remain authoritative there.
 *
 * ScriptPubKeyMan owns keypool organization and delegates new-key generation
 * to CWallet so existing wallet.dat and derivation behavior remain unchanged.
 */
class ScriptPubKeyMan
{
public:
    explicit ScriptPubKeyMan(CWallet* parent) : wallet(parent) {}
    virtual ~ScriptPubKeyMan() = default;

    //! Load a keypool entry from wallet.dat.
    void LoadKeyPool(int64_t index, const CKeyPool& keypool);

    //! Rewrite and refill the keypool.
    bool NewKeyPool();

    //! Top up the internal and external keypools.
    bool TopUp(unsigned int size = 0);

    //! Mark existing external keys as pre-split keys.
    void MarkPreSplitKeys();

    //! Mark keypool destinations found in a script as used.
    void MarkUnusedAddresses(const CScript& script);

    //! Return keypool statistics.
    size_t KeypoolCountExternalKeys() const;
    size_t KeypoolCountInternalKeys() const;
    unsigned int GetKeyPoolSize() const;
    int64_t GetOldestKeyPoolTime();

    //! Fetch or reserve keys from the managed keypool.
    bool GetKeyFromPool(CPubKey& key, bool internal = false);
    bool GetReservedKey(bool internal, int64_t& index, CKeyPool& keypool);
    bool ReserveKeyFromKeyPool(int64_t& index, CKeyPool& keypool, bool internal);

    //! Finalize or return a reserved destination.
    void KeepDestination(int64_t index);
    void ReturnDestination(int64_t index, bool internal);

    const std::map<CKeyID, int64_t>& GetAllReserveKeys() const
    {
        return m_pool_key_to_index;
    }

    //! Clear all in-memory keypool state after wallet database pool records are rewritten.
    void ClearKeyPool()
    {
        setInternalKeyPool.clear();
        setExternalKeyPool.clear();
        set_pre_split_keypool.clear();

        m_pool_key_to_index.clear();
        m_index_to_reserved_key.clear();

        m_max_keypool_index = 0;
    }

private:
    CWallet* wallet{nullptr};

    std::set<int64_t> setInternalKeyPool;
    std::set<int64_t> setExternalKeyPool;
    std::set<int64_t> set_pre_split_keypool;

    int64_t m_max_keypool_index{0};
    std::map<CKeyID, int64_t> m_pool_key_to_index;
    std::map<int64_t, CKeyID> m_index_to_reserved_key;

    void GeneratePool(CWalletDB& batch, int64_t target_size, bool internal);
    void AddKeypoolPubkeyWithDB(const CPubKey& pubkey, bool internal, CWalletDB& batch);
    void MarkReserveKeysAsUsed(int64_t keypool_id);
};

#endif // SCHILLINGCOIN_WALLET_SCRIPTPUBKEYMAN_H