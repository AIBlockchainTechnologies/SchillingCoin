// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "key_io.h"
#include "protocol.h"
#include "reverse_iterate.h"
#include "serialize.h"
#include "stubs/legacy_zerocoin_types.h"
#include "sync.h"
#include "txdb.h"
#include "uint256.h"
#include "util.h"
#include "util/string.h"
#include "util/system.h"
#include "utiltime.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "wallet/walletutil.h"

#include <algorithm>
#include <atomic>
#include <map>

#include <boost/filesystem.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

static uint64_t nAccountingEntryNumber = 0;

//
// WalletBatch
//

bool WalletBatch::WriteName(const std::string& strAddress, const std::string& strName)
{
    return WriteIC(std::make_pair(std::string("name"), strAddress), strName);
}

bool WalletBatch::EraseName(const std::string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    return EraseIC(std::make_pair(std::string("name"), strAddress));
}

bool WalletBatch::WritePurpose(const std::string& strAddress, const std::string& strPurpose)
{
    return WriteIC(std::make_pair(std::string("purpose"), strAddress), strPurpose);
}

bool WalletBatch::ErasePurpose(const std::string& strPurpose)
{
    return EraseIC(std::make_pair(std::string("purpose"), strPurpose));
}

bool WalletBatch::WriteTx(uint256 hash, const CWalletTx& wtx)
{
    return WriteIC(std::make_pair(std::string("tx"), hash), wtx);
}

bool WalletBatch::EraseTx(uint256 hash)
{
    return EraseIC(std::make_pair(std::string("tx"), hash));
}

bool WalletBatch::WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta)
{
    if (!WriteIC(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta, false)) {

        return false;
    }

    // Hash pubkey/privkey to accelerate wallet load.
    std::vector<unsigned char> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    return WriteIC(std::make_pair(std::string("key"), vchPubKey), std::make_pair(vchPrivKey, Hash(vchKey.begin(), vchKey.end())), false);

}

bool WalletBatch::WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata& keyMeta)
{
    const bool fEraseUnencryptedKey = true;

    if (!WriteIC(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta)) {

        return false;
    }

    if (!WriteIC(std::make_pair(std::string("ckey"), vchPubKey), vchCryptedSecret, false)) {

        return false;
    }

    if (fEraseUnencryptedKey) {
        EraseIC(std::make_pair(std::string("key"), vchPubKey));
        EraseIC(std::make_pair(std::string("wkey"), vchPubKey));
    }

    return true;
}

bool WalletBatch::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    return WriteIC(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
}

bool WalletBatch::WriteCScript(const uint160& hash, const CScript& redeemScript)
{
    return WriteIC(std::make_pair(std::string("cscript"), hash), redeemScript, false);
}

bool WalletBatch::WriteWatchOnly(const CScript& dest)
{
    return WriteIC(std::make_pair(std::string("watchs"), dest), '1');
}

bool WalletBatch::EraseWatchOnly(const CScript& dest)
{
    return EraseIC(std::make_pair(std::string("watchs"), dest));
}

bool WalletBatch::WriteMultiSig(const CScript& dest)
{
    return WriteIC(std::make_pair(std::string("multisig"), dest), '1');
}

bool WalletBatch::EraseMultiSig(const CScript& dest)
{
    return EraseIC(std::make_pair(std::string("multisig"), dest));
}

bool WalletBatch::WriteBestBlock(const CBlockLocator& locator)
{
    // Write an empty block locator so versions that require a merkle branch
    // automatically rescan.
    WriteIC(std::string("bestblock"), CBlockLocator());

    return WriteIC(std::string("bestblock_nomerkle"), locator);
}

bool WalletBatch::ReadBestBlock(CBlockLocator& locator)
{
    if (m_batch.Read(std::string("bestblock"), locator) &&
        !locator.vHave.empty()) {
        return true;
    }

    return m_batch.Read(std::string("bestblock_nomerkle"), locator);
}

bool WalletBatch::WriteOrderPosNext(int64_t nOrderPosNext)
{
    return WriteIC(std::string("orderposnext"), nOrderPosNext);
}

bool WalletBatch::WriteStakeSplitThreshold(CAmount nStakeSplitThreshold)
{
    return WriteIC(std::string("stakeSplitThreshold"), nStakeSplitThreshold);
}

// presstab HyperStake
bool WalletBatch::WriteMultiSend(std::vector<std::pair<std::string, int> > vMultiSend)
{
    bool ret = true;

    for (unsigned int i = 0; i < vMultiSend.size(); ++i) {
        const std::pair<std::string, int>& pMultiSend = vMultiSend[i];

        if (!WriteIC(
                std::make_pair(std::string("multisend"), i),
                pMultiSend,
                true)) {
            ret = false;
        }
    }

    return ret;
}

// presstab HyperStake
bool WalletBatch::EraseMultiSend(std::vector<std::pair<std::string, int> > vMultiSend)
{
    bool ret = true;

    for (unsigned int i = 0; i < vMultiSend.size(); ++i) {
        if (!EraseIC(
                std::make_pair(std::string("multisend"), i))) {
            ret = false;
        }
    }

    return ret;
}

// presstab HyperStake
bool WalletBatch::WriteMSettings(bool fMultiSendStake, bool fMultiSendMasternode, int nLastMultiSendHeight)
{
    const std::pair<bool, bool> enabledMS(fMultiSendStake, fMultiSendMasternode);

    const std::pair<std::pair<bool, bool>, int> pSettings(enabledMS, nLastMultiSendHeight);

    return WriteIC(std::string("msettingsv2"), pSettings, true);
}

// presstab HyperStake
bool WalletBatch::WriteMSDisabledAddresses(std::vector<std::string> vDisabledAddresses)
{
    bool ret = true;

    for (unsigned int i = 0; i < vDisabledAddresses.size(); ++i) {
        if (!WriteIC(
                std::make_pair(std::string("mdisabled"), i),
                vDisabledAddresses[i])) {
            ret = false;
        }
    }

    return ret;
}

// presstab HyperStake
bool WalletBatch::EraseMSDisabledAddresses(std::vector<std::string> vDisabledAddresses)
{
    bool ret = true;

    for (unsigned int i = 0; i < vDisabledAddresses.size(); ++i) {
        if (!EraseIC(
                std::make_pair(std::string("mdisabled"), i))) {
            ret = false;
        }
    }

    return ret;
}

bool WalletBatch::WriteAutoCombineSettings(bool fEnable, CAmount nCombineThreshold)
{
    std::pair<bool, CAmount> pSettings; pSettings.first = fEnable; pSettings.second = nCombineThreshold;

    return WriteIC(std::string("autocombinesettings"), pSettings, true);
}

bool WalletBatch::ReadPool(int64_t nPool, CKeyPool& keypool)
{
    return m_batch.Read(std::make_pair(std::string("pool"), nPool), keypool);
}

bool WalletBatch::WritePool(int64_t nPool, const CKeyPool& keypool)
{
    return WriteIC(std::make_pair(std::string("pool"), nPool), keypool);
}

bool WalletBatch::ErasePool(int64_t nPool)
{
    return EraseIC(std::make_pair(std::string("pool"), nPool));
}

bool WalletBatch::WriteMinVersion(int nVersion)
{
    return WriteIC(std::string("minversion"), nVersion);
}

bool WalletBatch::ReadAccount(const std::string& strAccount, CAccount& account)
{
    account.SetNull();

    return m_batch.Read(std::make_pair(std::string("acc"), strAccount), account);
}

bool WalletBatch::WriteAccount(const std::string& strAccount, const CAccount& account)
{
    return WriteIC(std::make_pair(std::string("acc"), strAccount), account);
}

bool WalletBatch::WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry)
{
    return WriteIC(std::make_pair(std::string("acentry"),
            std::make_pair(acentry.strAccount, nAccEntryNum)), acentry);
}

bool WalletBatch::WriteAccountingEntry_Backend(const CAccountingEntry& acentry)
{
    return WriteAccountingEntry(++nAccountingEntryNumber, acentry);
}

CAmount WalletBatch::GetAccountCreditDebit(const std::string& strAccount)
{
    std::list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    CAmount nCreditDebit = 0;
    for (const CAccountingEntry& entry : entries) {
        nCreditDebit += entry.nCreditDebit;
    }

    return nCreditDebit;
}

void WalletBatch::ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& entries)
{
    const bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = m_batch.GetCursor();
    if (!pcursor) {
        throw std::runtime_error(
            "WalletBatch::ListAccountCreditDebit() : cannot create DB cursor");
    }

    unsigned int fFlags = DB_SET_RANGE;

    while (true) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);

        if (fFlags == DB_SET_RANGE) {
            ssKey << std::make_pair(
                std::string("acentry"),
                std::make_pair(
                    fAllAccounts ? std::string("") : strAccount,
                    uint64_t(0)));
        }

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);

        const int ret =
            m_batch.ReadAtCursor(
                pcursor,
                ssKey,
                ssValue,
                fFlags);

        fFlags = DB_NEXT;

        if (ret == DB_NOTFOUND)
            break;

        if (ret != 0) {
            pcursor->close();

            throw std::runtime_error(
                "WalletBatch::ListAccountCreditDebit() : error scanning DB");
        }

        std::string strType;
        ssKey >> strType;

        if (strType != "acentry")
            break;

        CAccountingEntry acentry;

        ssKey >> acentry.strAccount;

        if (!fAllAccounts &&
            acentry.strAccount != strAccount)
            break;

        ssValue >> acentry;
        ssKey >> acentry.nEntryNo;

        entries.push_back(acentry);
    }

    pcursor->close();
}

DBErrors WalletBatch::ReorderTransactions(CWallet* pwallet)
{
    LOCK(pwallet->cs_wallet);
    // Old wallets didn't have any defined order for transactions
    // Probably a bad idea to change the output of this

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-time multimap.
    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair> TxItems;
    TxItems txByTime;

    for (std::map<uint256, CWalletTx>::iterator it = pwallet->mapWallet.begin(); it != pwallet->mapWallet.end(); ++it) {
        CWalletTx* wtx = &((*it).second);
        txByTime.insert(std::make_pair(wtx->nTimeReceived, TxPair(wtx, (CAccountingEntry*)0)));
    }
    std::list<CAccountingEntry> acentries;
    ListAccountCreditDebit("", acentries);
    for (CAccountingEntry& entry : acentries) {
        txByTime.insert(std::make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
    }

    int64_t& nOrderPosNext = pwallet->nOrderPosNext;
    nOrderPosNext = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it) {
        CWalletTx* const pwtx = (*it).second.first;
        CAccountingEntry* const pacentry = (*it).second.second;
        int64_t& nOrderPos = (pwtx != 0) ? pwtx->nOrderPos : pacentry->nOrderPos;

        if (nOrderPos == -1) {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (pwtx) {
                if (!WriteTx(pwtx->GetHash(), *pwtx))
                    return DB_LOAD_FAIL;
            } else if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                return DB_LOAD_FAIL;
        } else {
            int64_t nOrderPosOff = 0;
            for (const int64_t& nOffsetStart : nOrderPosOffsets) {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            // Since we're changing the order, write it back
            if (pwtx) {
                if (!WriteTx(pwtx->GetHash(), *pwtx))
                    return DB_LOAD_FAIL;
            } else if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                return DB_LOAD_FAIL;
        }
    }
    WriteOrderPosNext(nOrderPosNext);

    return DB_LOAD_OK;
}

class CWalletScanState
{
public:
    unsigned int nKeys;
    unsigned int nCKeys;
    unsigned int nKeyMeta;
    bool fIsEncrypted;
    bool fAnyUnordered;
    int nFileVersion;
    std::vector<uint256> vWalletUpgrade;

    CWalletScanState()
    {
        nKeys = nCKeys = nKeyMeta = 0;
        fIsEncrypted = false;
        fAnyUnordered = false;
        nFileVersion = 0;
    }
};

bool ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue, CWalletScanState& wss, std::string& strType, std::string& strErr)
{
    try {
        // Unserialize
        // Taking advantage of the fact that pair serialization
        // is just the two items serialized one after the other
        ssKey >> strType;
        if (strType == "name") {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[DecodeDestination(strAddress)].name;
        } else if (strType == "purpose") {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[DecodeDestination(strAddress)].purpose;
        } else if (strType == "tx") {
            uint256 hash;
            ssKey >> hash;
            CWalletTx wtx;
            ssValue >> wtx;
            if (wtx.GetHash() != hash)
                return false;

            // Undo serialize changes in 31600
            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703) {
                if (!ssValue.empty()) {
                    char fTmp;
                    char fUnused;
                    ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d '%s' %s",
                        wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                } else {
                    strErr = strprintf("LoadWallet() repairing tx ver=%d %s", wtx.fTimeReceivedIsTxTime, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                wss.vWalletUpgrade.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                wss.fAnyUnordered = true;

            pwallet->AddToWallet(wtx, true, nullptr);
        } else if (strType == "acentry") {
            std::string strAccount;
            ssKey >> strAccount;
            uint64_t nNumber;
            ssKey >> nNumber;
            if (nNumber > nAccountingEntryNumber)
                nAccountingEntryNumber = nNumber;

            if (!wss.fAnyUnordered) {
                CAccountingEntry acentry;
                ssValue >> acentry;
                if (acentry.nOrderPos == -1)
                    wss.fAnyUnordered = true;
            }
        } else if (strType == "watchs") {
            CScript script;
            ssKey >> script;
            char fYes;
            ssValue >> fYes;
            if (fYes == '1')
                pwallet->LoadWatchOnly(script);

            // Watch-only addresses have no birthday information for now,
            // so set the wallet birthday to the beginning of time.
            pwallet->nTimeFirstKey = 1;
        } else if (strType == "multisig") {
            CScript script;
            ssKey >> script;
            char fYes;
            ssValue >> fYes;
            if (fYes == '1')
                pwallet->LoadMultiSig(script);

            // MultiSig addresses have no birthday information for now,
            // so set the wallet birthday to the beginning of time.
            pwallet->nTimeFirstKey = 1;
        } else if (strType == "key" || strType == "wkey") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            CKey key;
            CPrivKey pkey;
            uint256 hash;

            if (strType == "key") {
                wss.nKeys++;
                ssValue >> pkey;
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                pkey = wkey.vchPrivKey;
            }

            // Old wallets store keys as "key" [pubkey] => [privkey]
            // ... which was slow for wallets with lots of keys, because the public key is re-derived from the private key
            // using EC operations as a checksum.
            // Newer wallets store keys as "key"[pubkey] => [privkey][hash(pubkey,privkey)], which is much faster while
            // remaining backwards-compatible.
            try {
                ssValue >> hash;
            } catch (...) {
            }

            bool fSkipCheck = false;

            if (!hash.IsNull()) {
                // hash pubkey/privkey to accelerate wallet load
                std::vector<unsigned char> vchKey;
                vchKey.reserve(vchPubKey.size() + pkey.size());
                vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
                vchKey.insert(vchKey.end(), pkey.begin(), pkey.end());

                if (Hash(vchKey.begin(), vchKey.end()) != hash) {
                    strErr = "Error reading wallet database: CPubKey/CPrivKey corrupt";
                    return false;
                }

                fSkipCheck = true;
            }

            if (!key.Load(pkey, vchPubKey, fSkipCheck)) {
                strErr = "Error reading wallet database: CPrivKey corrupt";
                return false;
            }
            if (!pwallet->LoadKey(key, vchPubKey)) {
                strErr = "Error reading wallet database: LoadKey failed";
                return false;
            }
        } else if (strType == "mkey") {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if (pwallet->mapMasterKeys.count(nID) != 0) {
                strErr = strprintf("Error reading wallet database: duplicate CMasterKey id %u", nID);
                return false;
            }
            pwallet->mapMasterKeys[nID] = kMasterKey;
            if (pwallet->nMasterKeyMaxID < nID)
                pwallet->nMasterKeyMaxID = nID;
        } else if (strType == "ckey") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            std::vector<unsigned char> vchPrivKey;
            ssValue >> vchPrivKey;
            wss.nCKeys++;

            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey)) {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        } else if (strType == "keymeta") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;

            pwallet->LoadKeyMetadata(vchPubKey, keyMeta);

            // find earliest key creation time, as wallet birthday
            if (!pwallet->nTimeFirstKey ||
                (keyMeta.nCreateTime < pwallet->nTimeFirstKey))
                pwallet->nTimeFirstKey = keyMeta.nCreateTime;
        } else if (strType == "defaultkey") {
            // We don't want or need the default key, but if there is one set,
            // we want to make sure that it is valid so that we can detect corruption
            CPubKey vchPubKey;
            ssValue >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: Default Key corrupt";
                return false;
            }
        } else if (strType == "pool") {
            int64_t nIndex;
            ssKey >> nIndex;
            CKeyPool keypool;
            ssValue >> keypool;
            pwallet->GetScriptPubKeyMan()->LoadKeyPool(nIndex, keypool);
        } else if (strType == "version") {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300)
                wss.nFileVersion = 300;
        } else if (strType == "cscript") {
            uint160 hash;
            ssKey >> hash;
            CScript script;
            ssValue >> script;
            if (!pwallet->LoadCScript(script)) {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
            }
        } else if (strType == "orderposnext") {
            ssValue >> pwallet->nOrderPosNext;
        } else if (strType == "stakeSplitThreshold") {
            ssValue >> pwallet->nStakeSplitThreshold;
            // originally saved as integer
            if (pwallet->nStakeSplitThreshold < COIN)
                pwallet->nStakeSplitThreshold *= COIN;
            // if ~1, force the value to the default to prevent thousands of accidental UTXO splits
            if (pwallet->nStakeSplitThreshold > 0 && pwallet->nStakeSplitThreshold <= 1 * COIN) {
                LogPrintf("WalletDB : nStakeSplitThreshold is too low (%u), setting to %u instead...\n", pwallet->nStakeSplitThreshold / COIN,
                                                                                                         (int)CWallet::DEFAULT_STAKE_SPLIT_THRESHOLD);
                pwallet->nStakeSplitThreshold = CWallet::DEFAULT_STAKE_SPLIT_THRESHOLD * COIN;
            }
        } else if (strType == "multisend") //presstab HyperStake
        {
            unsigned int i;
            ssKey >> i;
            std::pair<std::string, int> pMultiSend;
            ssValue >> pMultiSend;
            if (IsValidDestination(DecodeDestination(pMultiSend.first))) {
                pwallet->vMultiSend.push_back(pMultiSend);
            }
        } else if (strType == "msettingsv2") //presstab HyperStake
        {
            std::pair<std::pair<bool, bool>, int> pSettings;
            ssValue >> pSettings;
            pwallet->fMultiSendStake = pSettings.first.first;
            pwallet->fMultiSendMasternodeReward = pSettings.first.second;
            pwallet->nLastMultiSendHeight = pSettings.second;
        } else if (strType == "mdisabled") //presstab HyperStake
        {
            std::string strDisabledAddress;
            ssValue >> strDisabledAddress;
            pwallet->vDisabledAddresses.push_back(strDisabledAddress);
        } else if (strType == "autocombinesettings") {
            std::pair<bool, CAmount> pSettings;
            ssValue >> pSettings;
            pwallet->fCombineDust = pSettings.first;
            pwallet->nAutoCombineThreshold = pSettings.second;
        } else if (strType == "destdata") {
            std::string strAddress, strKey, strValue;
            ssKey >> strAddress;
            ssKey >> strKey;
            ssValue >> strValue;
            if (!pwallet->LoadDestData(DecodeDestination(strAddress), strKey, strValue)) {
                strErr = "Error reading wallet database: LoadDestData failed";
                return false;
            }
        }
        else if (strType == "hdchain")
        {
            CHDChain chain;
            ssValue >> chain;
            if (!pwallet->SetHDChain(chain, true))
            {
                strErr = "Error reading wallet database: SetHDChain failed";
                return false;
            }
        }
        else if (strType == "chdchain")
        {
            CHDChain chain;
            ssValue >> chain;
            if (!pwallet->SetCryptedHDChain(chain, true))
            {
                strErr = "Error reading wallet database: SetHDCryptedChain failed";
                return false;
            }
        }
        else if (strType == "hdpubkey")
        {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;

            CHDPubKey hdPubKey;
            ssValue >> hdPubKey;

            if(vchPubKey != hdPubKey.extPubKey.pubkey)
            {
                strErr = "Error reading wallet database: CHDPubKey corrupt";
                return false;
            }
            if (!pwallet->LoadHDPubKey(hdPubKey))
            {
                strErr = "Error reading wallet database: LoadHDPubKey failed";
                return false;
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool WalletBatch::IsKeyType(const std::string& strType)
{
    return (strType == "mkey" || strType == "ckey" || strType == "hdchain" || strType == "chdchain");
}

DBErrors WalletBatch::LoadWallet(CWallet* pwallet)
{
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    LOCK(pwallet->cs_wallet);

    try {
        int nMinVersion = 0;
        if (m_batch.Read(std::string("minversion"), nMinVersion)) {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;

            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = m_batch.GetCursor();
        if (!pcursor) {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true) {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);

            int ret = m_batch.ReadAtCursor(
                pcursor,
                ssKey,
                ssValue);

            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0) {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            // Try to be tolerant of single corrupt records:
            std::string strType, strErr;
            if (!ReadKeyValue(
                    pwallet,
                    ssKey,
                    ssValue,
                    wss,
                    strType,
                    strErr)) {
                // Losing keys is considered a catastrophic error, anything else
                // we assume the user can live with.
                if (IsKeyType(strType) || strType == "defaultkey") {
                    result = DB_CORRUPT;
                } else {
                    // Leave other errors alone. Trying to fix them here might
                    // make the database damage worse.
                    fNoncriticalErrors = true;

                    if (strType == "tx") {
                        // Rescan if there is a bad transaction record.
                        gArgs.SoftSetBoolArg("-rescan", true);
                    }
                }
            }

            if (!strErr.empty())
                LogPrintf("%s\n", strErr);
        }

        pcursor->close();
    } catch (const boost::thread_interrupted&) {
        throw;
    } catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    // Any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != DB_LOAD_OK)
        return result;

    LogPrintf("nFileVersion = %d\n", wss.nFileVersion);

    LogPrintf(
        "Keys: %u plaintext, %u encrypted, %u w/ metadata, %u total\n",
        wss.nKeys,
        wss.nCKeys,
        wss.nKeyMeta,
        wss.nKeys + wss.nCKeys);

    // nTimeFirstKey is only reliable if all keys have metadata.
    if ((wss.nKeys + wss.nCKeys) != wss.nKeyMeta)
        pwallet->nTimeFirstKey = 1; // 0 would be considered 'no value'

    for (uint256 hash : wss.vWalletUpgrade)
        WriteTx(hash, pwallet->mapWallet[hash]);

    // Rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc.
    if (wss.fIsEncrypted &&
        (wss.nFileVersion == 40000 ||
         wss.nFileVersion == 50000)) {
        return DB_NEED_REWRITE;
    }

    if (wss.nFileVersion < CLIENT_VERSION)
        WriteVersion(CLIENT_VERSION);

    if (wss.fAnyUnordered)
        result = ReorderTransactions(pwallet);

    pwallet->laccentries.clear();
    ListAccountCreditDebit("*", pwallet->laccentries);

    for (CAccountingEntry& entry : pwallet->laccentries) {
        pwallet->wtxOrdered.insert(
            std::make_pair(
                entry.nOrderPos,
                CWallet::TxPair(
                    (CWalletTx*)0,
                    &entry)));
    }

    return result;
}

DBErrors WalletBatch::FindWalletTx(CWallet* pwallet, std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx)
{
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    try {
        LOCK(pwallet->cs_wallet);

        int nMinVersion = 0;
        if (m_batch.Read(std::string("minversion"), nMinVersion)) {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;

            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = m_batch.GetCursor();
        if (!pcursor) {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true) {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);

            int ret = m_batch.ReadAtCursor(
                pcursor,
                ssKey,
                ssValue);

            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0) {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            std::string strType;
            ssKey >> strType;

            if (strType == "tx") {
                uint256 hash;
                ssKey >> hash;

                CWalletTx wtx;
                ssValue >> wtx;

                vTxHash.push_back(hash);
                vWtx.push_back(wtx);
            }
        }

        pcursor->close();
    } catch (const boost::thread_interrupted&) {
        throw;
    } catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    return result;
}

DBErrors WalletBatch::ZapWalletTx(CWallet* pwallet, std::vector<CWalletTx>& vWtx)
{
    // build list of wallet TXs
    std::vector<uint256> vTxHash;
    DBErrors err = FindWalletTx(pwallet, vTxHash, vWtx);
    if (err != DB_LOAD_OK)
        return err;

    // erase each wallet TX
    for (uint256& hash : vTxHash) {
        if (!EraseTx(hash))
            return DB_CORRUPT;
    }

    return DB_LOAD_OK;
}

void MaybeCompactWalletDB()
{
    static std::atomic<bool> fOneThread;

    if (fOneThread.exchange(true))
        return;

    if (!gArgs.GetBoolArg("-flushwallet", true)) {
        fOneThread = false;
        return;
    }

    if (pwalletMain) {
        WalletDatabase& database = pwalletMain->GetDBHandle();
        const unsigned int nUpdateCounter = database.nUpdateCounter.load();

        if (database.nLastSeen != nUpdateCounter) {
            database.nLastSeen = nUpdateCounter;
            database.nLastWalletUpdate = GetTime();
        }

        if (database.nLastFlushed != nUpdateCounter &&
            GetTime() - database.nLastWalletUpdate >= 2) {
            if (BerkeleyBatch::PeriodicFlush(database)) {
                database.nLastFlushed = nUpdateCounter;
            }
        }
    }

    fOneThread = false;
}

typedef std::multimap<std::time_t, fs::path> folder_set_t;

static folder_set_t buildBackupsMapSortedByLastWrite(const std::string& strWalletFile, const fs::path& backupsDir)
{
    folder_set_t folderSet;
    fs::directory_iterator endIter;

    // Build a map of backup files for the current wallet, sorted by last write time.
    for (fs::directory_iterator dirIter(backupsDir);
         dirIter != endIter;
         ++dirIter) {
        if (!fs::is_regular_file(dirIter->status())) {
            continue;
        }

        // Only include backups for the current wallet, for example wallet.dat.*.
        if (dirIter->path().stem().string() == strWalletFile) {
            folderSet.insert(folder_set_t::value_type(
                fs::last_write_time(dirIter->path()),
                *dirIter));
        }
    }

    return folderSet;
}

static bool cleanWalletBackups(folder_set_t& folderSet, int nWalletBackups, std::string& strBackupWarning)
{
    // Loop backward through backup files and keep the newest configured backups.
    int counter = 0;

    for (const std::pair<const std::time_t, fs::path>& file :
         reverse_iterate(folderSet)) {
        ++counter;

        if (counter <= nWalletBackups) {
            continue;
        }

        try {
            fs::remove(file.second);
            LogPrintf("Old backup deleted: %s\n", file.second);
        } catch (const fs::filesystem_error& error) {
            strBackupWarning = strprintf(
                _("Failed to delete backup, error: %s"),
                error.what());

            LogPrintf("%s\n", strBackupWarning);
            return false;
        }
    }

    return true;
}

bool AutoBackupWallet(CWallet& wallet, std::string& strBackupWarning, std::string& strBackupError)
{
    strBackupWarning.clear();
    strBackupError.clear();

    const int nWalletBackups = std::max(
        0,
        std::min(
            10,
            static_cast<int>(gArgs.GetArg(
                "-createwalletbackups",
                DEFAULT_CREATEWALLETBACKUPS))));

    if (nWalletBackups == 0) {
        LogPrintf("Automatic wallet backups are disabled!\n");
        return false;
    }

    fs::path backupsDir = GetDataDir() / "backups";
    backupsDir.make_preferred();

    TryCreateDirectories(backupsDir);

    const std::string strWalletFile =
        wallet.GetUniqueWalletBackupName();

    fs::path backupFile = backupsDir / strWalletFile;
    backupFile.make_preferred();

    if (fs::exists(backupFile)) {
        strBackupWarning = _(
            "Failed to create backup, file already exists! "
            "This could happen if the wallet was restarted within "
            "the backup filename's time resolution.");

        LogPrintf("%s\n", strBackupWarning);
        return false;
    }

    if (!wallet.BackupWallet(backupFile.string())) {
        strBackupError = _("Failed to backup wallet");
        return false;
    }

    // Keep only the configured number of backups, including the new backup.
    folder_set_t folderSet =
        buildBackupsMapSortedByLastWrite(
            backupFile.stem().string(),
            backupsDir);

    return cleanWalletBackups(
        folderSet,
        nWalletBackups,
        strBackupWarning);
}

//
// Try to carefully recover the wallet database if there is a problem.
//
bool WalletBatch::Recover(const fs::path& wallet_path, void* callbackDataIn,
bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& out_backup_filename)
{
    return BerkeleyBatch::Recover(wallet_path, callbackDataIn, recoverKVcallback, out_backup_filename);
}

bool WalletBatch::Recover(const fs::path& wallet_path, std::string& out_backup_filename)
{
    return WalletBatch::Recover(wallet_path, nullptr, nullptr, out_backup_filename);
}

bool WalletBatch::RecoverKeysOnlyFilter(void* callbackData, CDataStream ssKey, CDataStream ssValue)
{
    CWallet* dummyWallet =
        reinterpret_cast<CWallet*>(callbackData);

    if (!dummyWallet) {
        return false;
    }

    CWalletScanState dummyWss;
    std::string strType;
    std::string strErr;

    bool fReadOK;
    {
        LOCK(dummyWallet->cs_wallet);

        fReadOK = ReadKeyValue(
            dummyWallet,
            ssKey,
            ssValue,
            dummyWss,
            strType,
            strErr);
    }

    if (!IsKeyType(strType) &&
        strType != "hdpubkey") {
        return false;
    }

    if (!fReadOK) {
        LogPrintf(
            "WARNING: WalletBatch::Recover skipping %s: %s\n",
            strType,
            strErr);

        return false;
    }

    return true;
}

bool WalletBatch::VerifyEnvironment(const fs::path& wallet_path, std::string& errorStr)
{
    return BerkeleyBatch::VerifyEnvironment(wallet_path, errorStr);
}

bool WalletBatch::VerifyDatabaseFile(const fs::path& wallet_path, std::string& warningStr, std::string& errorStr)
{
    return BerkeleyBatch::VerifyDatabaseFile(wallet_path, warningStr, errorStr, static_cast<BerkeleyEnvironment::recoverFunc_type>(WalletBatch::Recover));
}

bool WalletBatch::WriteDestData(const std::string& address, const std::string& key, const std::string& value)
{
    return WriteIC(std::make_pair(std::string("destdata"), std::make_pair(address, key)), value);
}

bool WalletBatch::EraseDestData(const std::string& address, const std::string& key)
{
    return EraseIC(std::make_pair(std::string("destdata"), std::make_pair(address, key)));
}

bool WalletBatch::TxnBegin()
{
    return m_batch.TxnBegin();
}

bool WalletBatch::TxnCommit()
{
    return m_batch.TxnCommit();
}

bool WalletBatch::TxnAbort()
{
    return m_batch.TxnAbort();
}

bool WalletBatch::ReadVersion(int& nVersion)
{
    return m_batch.ReadVersion(nVersion);
}

bool WalletBatch::WriteVersion(int nVersion)
{
    return m_batch.WriteVersion(nVersion);
}

bool WalletBatch::WriteHDChain(const CHDChain& chain)
{
    return WriteIC(std::string("hdchain"), chain);
}

bool WalletBatch::WriteCryptedHDChain(const CHDChain& chain)
{
    if (!WriteIC(std::string("chdchain"), chain)) {

        return false;
    }

    EraseIC(std::string("hdchain"));

    return true;
}

bool WalletBatch::WriteHDPubKey(const CHDPubKey& hdPubKey, const CKeyMetadata& keyMeta)
{
    if (!WriteIC(
            std::make_pair(
                std::string("keymeta"),
                hdPubKey.extPubKey.pubkey),
            keyMeta,
            false)) {
        return false;
    }

    return WriteIC(
        std::make_pair(
            std::string("hdpubkey"),
            hdPubKey.extPubKey.pubkey),
        hdPubKey,
        false);
}

bool WalletBatch::WriteZerocoinSpendSerialEntry(const CZerocoinSpend& zerocoinSpend)
{
    return false; // Zerocoin removed
}

bool WalletBatch::EraseZerocoinSpendSerialEntry(const uint256& serialEntry)
{
    return false; // Zerocoin removed
}

bool WalletBatch::ReadZerocoinSpendSerialEntry(const uint256& bnSerial)
{
    return false; // Zerocoin removed
}

bool WalletBatch::WriteDeterministicMint(const CDeterministicMint& dMint)
{
    return false; // Zerocoin removed
}

bool WalletBatch::ReadDeterministicMint(const uint256& hashPubcoin, CDeterministicMint& dMint)
{
    return false; // Zerocoin removed
}

bool WalletBatch::EraseDeterministicMint(const uint256& hashPubcoin)
{
    return false; // Zerocoin removed
}

bool WalletBatch::WriteZerocoinMint(const CZerocoinMint& zerocoinMint)
{
    return false; // Zerocoin removed
}

bool WalletBatch::ReadZerocoinMint(const uint256& hashPubcoin, CZerocoinMint& mint)
{
    return false; // Zerocoin removed
}

bool WalletBatch::EraseZerocoinMint(const CZerocoinMint& zerocoinMint)
{
    return false; // Zerocoin removed
}

bool WalletBatch::ArchiveMintOrphan(const CZerocoinMint& zerocoinMint)
{
    return false; // Zerocoin removed
}

bool WalletBatch::ArchiveDeterministicOrphan(const CDeterministicMint& dMint)
{
    return false; // Zerocoin removed
}

bool WalletBatch::UnarchiveDeterministicMint(const uint256& hashPubcoin, CDeterministicMint& dMint)
{
    return false; // Zerocoin removed
}

bool WalletBatch::UnarchiveZerocoinMint(const uint256& hashPubcoin, CZerocoinMint& mint)
{
    return false; // Zerocoin removed
}

bool WalletBatch::WriteCurrentSeedHash(const uint256& hashSeed)
{
    return WriteIC(std::string("seedhash"), hashSeed);
}

bool WalletBatch::ReadCurrentSeedHash(uint256& hashSeed)
{
    return m_batch.Read(std::string("seedhash"), hashSeed);
}

bool WalletBatch::WriteZSCHSeed(const uint256& hashSeed, const std::vector<unsigned char>& seed)
{
    if (!WriteCurrentSeedHash(hashSeed)) return error("%s: failed to write current seed hash", __func__);

    return WriteIC(std::make_pair(std::string("dzs"), hashSeed), seed);
}

bool WalletBatch::EraseZSCHSeed()
{
    uint256 hash;

    if (!ReadCurrentSeedHash(hash)) {
        return error("Failed to read a current seed hash");
    }

    if (!WriteZSCHSeed(
            hash,
            ToByteVector(base_uint<256>(0) << 256))) {
        return error("Failed to write empty seed to wallet");
    }

    if (!WriteCurrentSeedHash(UINT256_ZERO)) {
        return error("Failed to write empty seedHash");
    }

    return true;
}

bool WalletBatch::EraseZSCHSeed_deprecated()
{
    return EraseIC(std::string("dzs"));
}

bool WalletBatch::ReadZSCHSeed(const uint256& hashSeed, std::vector<unsigned char>& seed)
{
    return m_batch.Read(std::make_pair(std::string("dzs"), hashSeed), seed);
}

bool WalletBatch::ReadZSCHSeed_deprecated(uint256& seed)
{
    return m_batch.Read(std::string("dzs"), seed);
}

bool WalletBatch::WriteZSCHCount(const uint32_t& nCount)
{
    return WriteIC(std::string("dzc"), nCount);
}

bool WalletBatch::ReadZSCHCount(uint32_t& nCount)
{
    return m_batch.Read(std::string("dzc"), nCount);
}

bool WalletBatch::WriteMintPoolPair(const uint256& hashMasterSeed, const uint256& hashPubcoin, const uint32_t& nCount)
{
    return WriteIC(std::make_pair(std::string("mintpool"), hashPubcoin), std::make_pair(hashMasterSeed, nCount));
}

//! map with hashMasterSeed as the key, paired with vector of hashPubcoins and their count
std::map<uint256, std::vector<std::pair<uint256, uint32_t> > > WalletBatch::MapMintPool()
{
    std::map<uint256, std::vector<std::pair<uint256, uint32_t> > > mapPool;

    Dbc* pcursor = m_batch.GetCursor();
    if (!pcursor) {
        throw std::runtime_error(
            std::string(__func__) + " : cannot create DB cursor");
    }

    unsigned int fFlags = DB_SET_RANGE;

    for (;;) {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE) {
            ssKey << std::make_pair(
                std::string("mintpool"),
                UINT256_ZERO);
        }

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);

        int ret = m_batch.ReadAtCursor(
            pcursor,
            ssKey,
            ssValue,
            fFlags);

        fFlags = DB_NEXT;

        if (ret == DB_NOTFOUND) {
            break;
        } else if (ret != 0) {
            pcursor->close();

            throw std::runtime_error(
                std::string(__func__) + " : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;

        if (strType != "mintpool") {
            break;
        }

        uint256 hashPubcoin;
        ssKey >> hashPubcoin;

        uint256 hashMasterSeed;
        ssValue >> hashMasterSeed;

        uint32_t nCount;
        ssValue >> nCount;

        std::pair<uint256, uint32_t> pMint;
        pMint.first = hashPubcoin;
        pMint.second = nCount;

        if (mapPool.count(hashMasterSeed)) {
            mapPool.at(hashMasterSeed).emplace_back(pMint);
        } else {
            std::vector<std::pair<uint256, uint32_t> > vPairs;
            vPairs.emplace_back(pMint);

            mapPool.insert(
                std::make_pair(
                    hashMasterSeed,
                    vPairs));
        }
    }

    pcursor->close();

    return mapPool;
}

std::list<CDeterministicMint> WalletBatch::ListDeterministicMints()
{
    std::list<CDeterministicMint> listMints;

    Dbc* pcursor = m_batch.GetCursor();
    if (!pcursor) {
        throw std::runtime_error(
            std::string(__func__) + " : cannot create DB cursor");
    }

    unsigned int fFlags = DB_SET_RANGE;

    for (;;) {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);

        if (fFlags == DB_SET_RANGE) {
            ssKey << make_pair(
                std::string("dzsch"),
                UINT256_ZERO);
        }

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);

        int ret = m_batch.ReadAtCursor(
            pcursor,
            ssKey,
            ssValue,
            fFlags);

        fFlags = DB_NEXT;

        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0) {
            pcursor->close();

            throw std::runtime_error(
                std::string(__func__) +
                " : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;

        if (strType != "dzsch")
            break;

        uint256 hashPubcoin;
        ssKey >> hashPubcoin;

        CDeterministicMint mint;
        ssValue >> mint;

        listMints.emplace_back(mint);
    }

    pcursor->close();

    return listMints;
}

std::list<CZerocoinMint> WalletBatch::ListMintedCoins()
{
    return std::list<CZerocoinMint>(); // Zerocoin removed
}

std::list<CZerocoinSpend> WalletBatch::ListSpentCoins()
{
    return std::list<CZerocoinSpend>(); // Zerocoin removed
}

std::list<uint256> WalletBatch::ListSpentCoinsSerial()
{
    return std::list<uint256>(); // Zerocoin removed
}

std::list<CZerocoinMint> WalletBatch::ListArchivedZerocoins()
{
    return std::list<CZerocoinMint>(); // Zerocoin removed
}

std::list<CDeterministicMint> WalletBatch::ListArchivedDeterministicMints()
{
    std::list<CDeterministicMint> listMints;

    Dbc* pcursor = m_batch.GetCursor();
    if (!pcursor) {
        throw std::runtime_error(
            std::string(__func__) + " : cannot create DB cursor");
    }

    unsigned int fFlags = DB_SET_RANGE;

    for (;;) {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);

        if (fFlags == DB_SET_RANGE) {
            ssKey << make_pair(
                std::string("dzco"),
                UINT256_ZERO);
        }

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);

        int ret = m_batch.ReadAtCursor(
            pcursor,
            ssKey,
            ssValue,
            fFlags);

        fFlags = DB_NEXT;

        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();

            throw std::runtime_error(
                std::string(__func__) + " : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;

        if (strType != "dzco")
            break;

        uint256 value;
        ssKey >> value;

        CDeterministicMint dMint;
        ssValue >> dMint;

        listMints.emplace_back(dMint);
    }

    pcursor->close();

    return listMints;
}