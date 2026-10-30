// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_BDB_H
#define SCHILLINGCOIN_WALLET_BDB_H

#include "clientversion.h"
#include "serialize.h"
#include "streams.h"
#include "support/cleanse.h"
#include "sync.h"
#include "util/fs.h"
#include "version.h"
#include "wallet/db.h"

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <db_cxx.h>

class CDiskBlockIndex;
class COutPoint;

struct CBlockLocator;

static const unsigned int DEFAULT_WALLET_DBLOGSIZE = 100;
static const bool DEFAULT_WALLET_PRIVDB = true;

struct WalletDatabaseFileId
{
    u_int8_t value[DB_FILE_ID_LEN];

    bool operator==(const WalletDatabaseFileId& rhs) const;
};

class BerkeleyEnvironment
{
private:
    bool fDbEnvInit;
    bool fMockDb;

    // Don't change into fs::path, as that can result in
    // shutdown problems/crashes caused by a static initialized internal pointer.
    std::string strPath;

public:
    std::unique_ptr<DbEnv> dbenv;
    std::map<std::string, int> mapFileUseCount;
    std::map<std::string, Db*> mapDb;
    std::unordered_map<std::string, WalletDatabaseFileId> m_fileids;
    std::condition_variable_any m_db_in_use;

    explicit BerkeleyEnvironment(const fs::path& env_directory);
    ~BerkeleyEnvironment();

    void Reset();
    void MakeMock();

    bool IsMock() const
    {
        return fMockDb;
    }

    bool IsInitialized() const
    {
        return fDbEnvInit;
    }

    fs::path Directory() const
    {
        return strPath;
    }

    enum VerifyResult {
        VERIFY_OK,
        RECOVER_OK,
        RECOVER_FAIL
    };

    typedef bool (*recoverFunc_type)(
        const fs::path& file_path,
        std::string& out_backup_filename);

    VerifyResult Verify(
        const std::string& strFile,
        recoverFunc_type recoverFunc,
        std::string& out_backup_filename);

    typedef std::pair<
        std::vector<unsigned char>,
        std::vector<unsigned char>
    > KeyValPair;

    bool Salvage(
        const std::string& strFile,
        bool fAggressive,
        std::vector<KeyValPair>& vResult);

    bool Open(bool retry);
    void Close();
    void Flush(bool fShutdown);
    void CheckpointLSN(const std::string& strFile);
    void ResetLSN(const std::string& strFile);
    void CloseDb(const std::string& strFile);
    void ReloadDbEnv();

    DbTxn* TxnBegin(int flags = DB_TXN_WRITE_NOSYNC)
    {
        DbTxn* ptxn = nullptr;
        int ret = dbenv->txn_begin(nullptr, &ptxn, flags);

        if (!ptxn || ret != 0)
            return nullptr;

        return ptxn;
    }
};

/** Resolve the BerkeleyDB data file for a wallet path. */
fs::path BDBDataFile(const fs::path& wallet_path);

/** Get BerkeleyEnvironment and database filename given a wallet path. */
BerkeleyEnvironment* GetWalletEnv(const fs::path& wallet_path, std::string& database_filename);

/**
 * An instance of this class represents one database.
 * For BerkeleyDB this is just an (env, strFile) tuple.
 */
class BerkeleyDatabase : public WalletDatabase
{
    friend class BerkeleyBatch;

public:
    /** Create dummy DB handle. */
    BerkeleyDatabase()
        : WalletDatabase(),
          env(nullptr)
    {
    }

    /** Create DB handle to real database. */
    explicit BerkeleyDatabase(
        const fs::path& wallet_path,
        bool mock = false)
        : WalletDatabase()
    {
        env = GetWalletEnv(wallet_path, strFile);

        if (mock) {
            env->Close();
            env->Reset();
            env->MakeMock();
        }
    }

    /** Rewrite the entire database on disk, except for pszSkip if provided. */
    bool Rewrite(
        const char* pszSkip = nullptr) override;

    /** Back up the entire database to a file. */
    bool Backup(
        const std::string& strDest) override;

    /** Flush database changes, optionally as part of shutdown. */
    void Flush(
        bool shutdown) override;

    /** Passively flush the wallet database when it is safe to do so. */
    bool PeriodicFlush() override;

    void IncrementUpdateCounter() override;
    void ReloadDbEnv() override;

    fs::path GetPathToFile() const override
    {
        if (IsDummy()) {
            return fs::path();
        }

        return env->Directory() / strFile;
    }

    std::string Filename() const override
    {
        return strFile;
    }

    std::string Format() const override
    {
        return "bdb";
    }

    std::unique_ptr<DatabaseBatch> MakeBatch(
        const char* mode = "r+",
        bool flush_on_close = true) override;

private:
    /** BerkeleyDB-specific ownership. */
    BerkeleyEnvironment* env;
    std::string strFile;

    /**
     * Return whether this database handle is a dummy for testing.
     * Only low-level database code should need to inspect this.
     */
    bool IsDummy() const
    {
        return env == nullptr;
    }
};

/** RAII class that provides access to a Berkeley database. */
class BerkeleyBatch : public DatabaseBatch
{
private:
    bool ReadKey(
        CDataStream&& key,
        CDataStream& value) override;

    bool WriteKey(
        CDataStream&& key,
        CDataStream&& value,
        bool overwrite) override;

    bool EraseKey(
        CDataStream&& key) override;

    bool HasKey(
        CDataStream&& key) override;

protected:
    Db* pdb;
    std::string strFile;
    DbTxn* activeTxn;
    bool fReadOnly;
    bool fFlushOnClose;
    BerkeleyEnvironment* env;
    Dbc* m_cursor;

public:
    explicit BerkeleyBatch(
        BerkeleyDatabase& database,
        const char* pszMode = "r+",
        bool fFlushOnCloseIn = true);

    ~BerkeleyBatch() override
    {
        Close();
    }

    BerkeleyBatch(const BerkeleyBatch&) = delete;
    BerkeleyBatch& operator=(const BerkeleyBatch&) = delete;

    void Flush() override;
    void Close() override;

    bool StartCursor() override;

    bool ReadAtCursor(
        CDataStream& ssKey,
        CDataStream& ssValue,
        bool setRange,
        bool& complete) override;

    void CloseCursor() override;

    bool TxnBegin() override
    {
        if (!pdb || activeTxn) {
            return false;
        }

        DbTxn* ptxn = env->TxnBegin();
        if (!ptxn) {
            return false;
        }

        activeTxn = ptxn;
        return true;
    }

    bool TxnCommit() override
    {
        if (!pdb || !activeTxn) {
            return false;
        }

        const int ret = activeTxn->commit(0);
        activeTxn = nullptr;

        return ret == 0;
    }

    bool TxnAbort() override
    {
        if (!pdb || !activeTxn) {
            return false;
        }

        const int ret = activeTxn->abort();
        activeTxn = nullptr;

        return ret == 0;
    }

    static bool Recover(
        const fs::path& file_path,
        void* callbackDataIn,
        bool (*recoverKVcallback)(
            void* callbackData,
            CDataStream ssKey,
            CDataStream ssValue),
        std::string& out_backup_filename);

    /**
     * Flush the wallet passively using TRY_LOCK.
     * Intended to be called periodically.
     */
    static bool PeriodicFlush(
        BerkeleyDatabase& database);

    /** Verify the database environment. */
    static bool VerifyEnvironment(
        const fs::path& file_path,
        std::string& errorStr);

    /** Verify the database file. */
    static bool VerifyDatabaseFile(
        const fs::path& file_path,
        std::string& warningStr,
        std::string& errorStr,
        BerkeleyEnvironment::recoverFunc_type recoverFunc);

    static bool Rewrite(
        BerkeleyDatabase& database,
        const char* pszSkip = nullptr);
};

std::unique_ptr<BerkeleyDatabase> MakeBerkeleyDatabase(const fs::path& path, const DatabaseOptions& options, DatabaseStatus& status, std::string& error);

std::unique_ptr<WalletDatabase> MakeMockBerkeleyDatabase();

#endif // SCHILLINGCOIN_WALLET_BDB_H