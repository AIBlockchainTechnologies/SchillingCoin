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
#include "sync.h"
#include "util/fs.h"
#include "wallet/db.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <db_cxx.h>

struct bilingual_str;

#define BDB_DB_FILE_ID_LEN 20

namespace wallet {

struct WalletDatabaseFileId
{
    uint8_t value[BDB_DB_FILE_ID_LEN];

    bool operator==(const WalletDatabaseFileId& rhs) const;
};

class BerkeleyDatabase;

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
    std::map<fs::path, std::reference_wrapper<BerkeleyDatabase>> m_databases;
    std::unordered_map<std::string, WalletDatabaseFileId> m_fileids;
    std::condition_variable_any m_db_in_use;
    bool m_use_shared_memory;

    explicit BerkeleyEnvironment(
        const fs::path& env_directory,
        bool use_shared_memory);

    BerkeleyEnvironment();

    ~BerkeleyEnvironment();

    void Reset();

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
        return fs::PathFromString(strPath);
    }

    enum VerifyResult {
        VERIFY_OK,
        RECOVER_OK,
        RECOVER_FAIL
    };

    using recoverFunc_type = bool (*)(
        const fs::path& file_path,
        std::string& out_backup_filename);

    VerifyResult Verify(
        const std::string& strFile,
        recoverFunc_type recoverFunc,
        std::string& out_backup_filename);

    using KeyValPair = std::pair<
        std::vector<unsigned char>,
        std::vector<unsigned char>>;

    bool Salvage(
        const std::string& strFile,
        bool fAggressive,
        std::vector<KeyValPair>& vResult);

    bool Open(bilingual_str& error);
    void Close();
    void Flush(bool fShutdown);
    void CheckpointLSN(const std::string& strFile);
    void ResetLSN(const std::string& strFile);
    void CloseDb(const fs::path& filename);
    void ReloadDbEnv();
    DbTxn* TxnBegin(int flags = DB_TXN_WRITE_NOSYNC);
};

/** Get BerkeleyEnvironment given a directory path. */
std::shared_ptr<BerkeleyEnvironment> GetBerkeleyEnv(
    const fs::path& env_directory,
    bool use_shared_memory);

class BerkeleyBatch;

/**
 * An instance of this class represents one Berkeley database.
 */
class BerkeleyDatabase : public WalletDatabase
{
public:
    BerkeleyDatabase() = delete;

    BerkeleyDatabase(
        std::shared_ptr<BerkeleyEnvironment> env,
        fs::path filename,
        const DatabaseOptions& options);

    ~BerkeleyDatabase() override;

    /** Open the database if it is not already open. */
    void Open() override;

    /** Rewrite the complete database except for pszSkip when specified. */
    bool Rewrite(
        const char* pszSkip = nullptr) override;

    /** Indicate that a new database user has begun using the database. */
    void AddRef() override;

    /** Indicate that a database user has stopped using the database. */
    void RemoveRef() override;

    /** Back up the complete database. */
    bool Backup(
        const std::string& strDest) const override;

    /** Flush outstanding changes to the database file. */
    void Flush() override;

    /**
     * Flush and close the database. Also close the environment when no other
     * database in that environment remains open.
     */
    void Close() override;

    /** Passively flush the database when no active batch prevents it. */
    bool PeriodicFlush() override;

    void IncrementUpdateCounter() override;
    void ReloadDbEnv() override;

    /** Verify the environment and database file. */
    bool Verify(bilingual_str& error);

    std::string Filename() override
    {
        return fs::PathToString(env->Directory() / m_filename);
    }

    std::vector<fs::path> Files() override
    {
        return {env->Directory() / m_filename};
    }

    std::string Format() override
    {
        return "bdb";
    }

    /**
     * Pointer to the shared Berkeley environment.
     *
     * Multiple legacy BDB wallet files in the same directory may share one
     * environment. The environment remains alive until its last database is
     * released.
     */
    std::shared_ptr<BerkeleyEnvironment> env;

    /**
     * Database handle. It is opened lazily and reset during flush or close
     * operations.
     */
    std::unique_ptr<Db> m_db;

    fs::path m_filename;
    int64_t m_max_log_mb;

    std::unique_ptr<DatabaseBatch> MakeBatch(
        bool flush_on_close = true) override;
};

/** RAII object that automatically cleanses Berkeley DB data on destruction. */
class SafeDbt final
{
private:
    Dbt m_dbt;

public:
    SafeDbt();
    SafeDbt(void* data, size_t size);
    ~SafeDbt();

    const void* get_data() const;
    void set_data(void* data, size_t size);
    uint32_t get_size() const;

    operator Dbt*();
};

/** Backend cursor for a Berkeley database. */
class BerkeleyCursor : public DatabaseCursor
{
private:
    Dbc* m_cursor;
    std::vector<std::byte> m_key_prefix;
    bool m_first{true};

public:
    /**
     * Construct a cursor matching prefix. An empty prefix matches all records.
     */
    explicit BerkeleyCursor(
        BerkeleyDatabase& database,
        const BerkeleyBatch& batch,
        std::span<const std::byte> prefix = {});

    ~BerkeleyCursor() override;

    Status Next(
        DataStream& key,
        DataStream& value) override;

    Dbc* dbc() const
    {
        return m_cursor;
    }
};

/** RAII object providing batch access to a Berkeley database. */
class BerkeleyBatch : public DatabaseBatch
{
public:
    bool ReadKey(
        DataStream&& key,
        DataStream& value) override;

    bool WriteKey(
        DataStream&& key,
        DataStream&& value,
        bool overwrite = true) override;

    bool EraseKey(
        DataStream&& key) override;

    bool HasKey(
        DataStream&& key) override;

    bool ErasePrefix(
        std::span<const std::byte> prefix) override;

    /*
     * These members remain accessible for SCH's Particl-derived CHDWalletDB
     * raw-cursor bridge.
     */
    Db* pdb{nullptr};
    std::string strFile;
    DbTxn* activeTxn{nullptr};
    bool fReadOnly;
    bool fFlushOnClose;
    BerkeleyEnvironment* env;
    BerkeleyDatabase& m_database;

    explicit BerkeleyBatch(
        BerkeleyDatabase& database,
        bool read_only,
        bool flush_on_close = true);

    ~BerkeleyBatch() override;

    BerkeleyBatch(const BerkeleyBatch&) = delete;
    BerkeleyBatch& operator=(const BerkeleyBatch&) = delete;

    void Flush() override;
    void Close() override;

    std::unique_ptr<DatabaseCursor> GetNewCursor() override;

    std::unique_ptr<DatabaseCursor> GetNewPrefixCursor(
        std::span<const std::byte> prefix) override;

    bool TxnBegin() override;
    bool TxnCommit() override;
    bool TxnAbort() override;

    bool HasActiveTxn() override
    {
        return activeTxn != nullptr;
    }

    DbTxn* txn() const
    {
        return activeTxn;
    }

    /**
     * Particl-derived raw cursor bridge retained for CHDWalletDB.
     */
    int ReadAtCursor2(
        Dbc* pcursor,
        DataStream& ssKey,
        DataStream& ssValue,
        bool setRange = false);

    static bool Recover(
        const fs::path& file_path,
        void* callbackDataIn,
        bool (*recoverKVcallback)(
            void* callbackData,
            CDataStream ssKey,
            CDataStream ssValue),
        std::string& out_backup_filename);

    static bool VerifyEnvironment(
        const fs::path& file_path,
        std::string& errorStr);

    static bool VerifyDatabaseFile(
        const fs::path& file_path,
        std::string& warningStr,
        std::string& errorStr,
        BerkeleyEnvironment::recoverFunc_type recoverFunc);
};

std::string BerkeleyDatabaseVersion();

/** Perform a runtime sanity check against the linked Berkeley DB version. */
bool BerkeleyDatabaseSanityCheck();

/** Return a Berkeley database at the specified wallet path. */
std::unique_ptr<BerkeleyDatabase> MakeBerkeleyDatabase(
    const fs::path& path,
    const DatabaseOptions& options,
    DatabaseStatus& status,
    bilingual_str& error);

} // namespace wallet

#endif // SCHILLINGCOIN_WALLET_BDB_H