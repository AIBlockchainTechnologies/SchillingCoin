// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_DB_H
#define SCHILLINGCOIN_WALLET_DB_H

#include "clientversion.h"
#include "streams.h"
#include "support/allocators/secure.h"
#include "util/fs.h"

#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

class ArgsManager;
struct bilingual_str;

namespace wallet {

class DatabaseCursor
{
public:
    DatabaseCursor() = default;
    virtual ~DatabaseCursor() = default;

    DatabaseCursor(const DatabaseCursor&) = delete;
    DatabaseCursor& operator=(const DatabaseCursor&) = delete;

    enum class Status
    {
        FAIL,
        MORE,
        DONE,
    };

    virtual Status Next(DataStream& key, DataStream& value)
    {
        return Status::FAIL;
    }
};

/**
 * Backend-independent database batch interface.
 *
 * Concrete database backends implement serialized key/value operations,
 * transaction handling, and cursor ownership. The public template functions
 * preserve SCH wallet-record serialization.
 */
class DatabaseBatch
{
private:
    virtual bool ReadKey(
        DataStream&& key,
        DataStream& value) = 0;

    virtual bool WriteKey(
        DataStream&& key,
        DataStream&& value,
        bool overwrite = true) = 0;

    virtual bool EraseKey(
        DataStream&& key) = 0;

    virtual bool HasKey(
        DataStream&& key) = 0;

public:
    DatabaseBatch() = default;
    virtual ~DatabaseBatch() = default;

    DatabaseBatch(const DatabaseBatch&) = delete;
    DatabaseBatch& operator=(const DatabaseBatch&) = delete;

    template <typename K, typename T>
    bool Read(const K& key, T& value)
    {
        DataStream key_stream{};
        key_stream.reserve(1000);
        key_stream << key;

        DataStream value_stream{};

        if (!ReadKey(
                std::move(key_stream),
                value_stream)) {
            return false;
        }

        try {
            value_stream >> value;
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    template <typename K, typename T>
    bool Write(
        const K& key,
        const T& value,
        bool overwrite = true)
    {
        DataStream key_stream{};
        key_stream.reserve(1000);
        key_stream << key;

        DataStream value_stream{};
        value_stream.reserve(10000);
        value_stream << value;

        return WriteKey(
            std::move(key_stream),
            std::move(value_stream),
            overwrite);
    }

    template <typename K>
    bool Erase(const K& key)
    {
        DataStream key_stream{};
        key_stream.reserve(1000);
        key_stream << key;

        return EraseKey(std::move(key_stream));
    }

    template <typename K>
    bool Exists(const K& key)
    {
        DataStream key_stream{};
        key_stream.reserve(1000);
        key_stream << key;

        return HasKey(std::move(key_stream));
    }

    virtual void Flush() = 0;
    virtual void Close() = 0;

    virtual bool ErasePrefix(
        std::span<const std::byte> prefix) = 0;

    virtual std::unique_ptr<DatabaseCursor> GetNewCursor() = 0;

    virtual std::unique_ptr<DatabaseCursor> GetNewPrefixCursor(
        std::span<const std::byte> prefix) = 0;

    virtual bool TxnBegin() = 0;
    virtual bool TxnCommit() = 0;
    virtual bool TxnAbort() = 0;
    virtual bool HasActiveTxn() = 0;

    bool ReadVersion(int& version)
    {
        version = 0;
        return Read(std::string("version"), version);
    }

    bool WriteVersion(int version)
    {
        return Write(std::string("version"), version);
    }
};

enum class DatabaseFormat
{
    BERKELEY,
    SQLITE,
};

enum class DatabaseStatus
{
    SUCCESS,
    FAILED_BAD_PATH,
    FAILED_BAD_FORMAT,
    FAILED_ALREADY_LOADED,
    FAILED_ALREADY_EXISTS,
    FAILED_NOT_FOUND,
    FAILED_CREATE,
    FAILED_LOAD,
    FAILED_VERIFY,
    FAILED_ENCRYPT,
    FAILED_INVALID_BACKUP_FILE,
};

struct DatabaseOptions
{
    bool require_existing{false};
    bool require_create{false};
    std::optional<DatabaseFormat> require_format;
    uint64_t create_flags{0};
    SecureString create_passphrase;

    // Specialized options. Not every option is supported by every backend.
    bool verify{true};
    bool use_unsafe_sync{false};
    bool use_shared_memory{false};
    int64_t max_log_mb{100};
};

/**
 * Backend-independent wallet database interface.
 */
class WalletDatabase
{
public:
    WalletDatabase() = default;
    virtual ~WalletDatabase() = default;

    WalletDatabase(const WalletDatabase&) = delete;
    WalletDatabase& operator=(const WalletDatabase&) = delete;

    /** Open the database if it is not already open. */
    virtual void Open() = 0;

    /** Counts active database users so the database is not closed while in use. */
    std::atomic<int> m_refcount{0};

    /** Indicate that a new database user has begun using the database. */
    virtual void AddRef() = 0;

    /** Indicate that a database user has stopped using the database. */
    virtual void RemoveRef() = 0;

    /**
     * Rewrite the complete database, except for records matching skip when
     * supported by the backend.
     */
    virtual bool Rewrite(
        const char* skip = nullptr) = 0;

    /** Back up the complete database. */
    virtual bool Backup(
        const std::string& destination) const = 0;

    /** Flush outstanding changes without closing the database. */
    virtual void Flush() = 0;

    /** Flush and close the database. */
    virtual void Close() = 0;

    /** Passively flush the database when no active batch prevents it. */
    virtual bool PeriodicFlush() = 0;

    virtual void IncrementUpdateCounter() = 0;
    virtual void ReloadDbEnv() = 0;

    /** Return the path used for logs and error messages. */
    virtual std::string Filename() = 0;

    /** Return all files owned by this database. */
    virtual std::vector<fs::path> Files() = 0;

    /** Return the persistent database format name. */
    virtual std::string Format() = 0;

    /** Create a batch connected to this database. */
    virtual std::unique_ptr<DatabaseBatch> MakeBatch(
        bool flush_on_close = true) = 0;

    std::atomic<unsigned int> nUpdateCounter{0};
    unsigned int nLastSeen{0};
    unsigned int nLastFlushed{0};
    int64_t nLastWalletUpdate{0};
};

class DummyCursor final : public DatabaseCursor
{
public:
    Status Next(
        DataStream&,
        DataStream&) override
    {
        return Status::DONE;
    }
};

/**
 * Database batch implementation with no persistent storage.
 */
class DummyBatch final : public DatabaseBatch
{
private:
    bool ReadKey(
        DataStream&&,
        DataStream&) override
    {
        return false;
    }

    bool WriteKey(
        DataStream&&,
        DataStream&&,
        bool) override
    {
        return true;
    }

    bool EraseKey(
        DataStream&&) override
    {
        return true;
    }

    bool HasKey(
        DataStream&&) override
    {
        return false;
    }

public:
    void Flush() override
    {
    }

    void Close() override
    {
    }

    bool ErasePrefix(
        std::span<const std::byte>) override
    {
        return true;
    }

    std::unique_ptr<DatabaseCursor> GetNewCursor() override
    {
        return std::make_unique<DummyCursor>();
    }

    std::unique_ptr<DatabaseCursor> GetNewPrefixCursor(
        std::span<const std::byte>) override
    {
        return std::make_unique<DummyCursor>();
    }

    bool TxnBegin() override
    {
        return true;
    }

    bool TxnCommit() override
    {
        return true;
    }

    bool TxnAbort() override
    {
        return true;
    }

    bool HasActiveTxn() override
    {
        return false;
    }
};

/**
 * Dummy wallet database used when persistent storage is unnecessary.
 */
class DummyDatabase final : public WalletDatabase
{
public:
    void Open() override
    {
    }

    void AddRef() override
    {
    }

    void RemoveRef() override
    {
    }

    bool Rewrite(
        const char* = nullptr) override
    {
        return true;
    }

    bool Backup(
        const std::string&) const override
    {
        return true;
    }

    void Flush() override
    {
    }

    void Close() override
    {
    }

    bool PeriodicFlush() override
    {
        return true;
    }

    void IncrementUpdateCounter() override
    {
        ++nUpdateCounter;
    }

    void ReloadDbEnv() override
    {
    }

    std::string Filename() override
    {
        return "dummy";
    }

    std::vector<fs::path> Files() override
    {
        return {};
    }

    std::string Format() override
    {
        return "dummy";
    }

    std::unique_ptr<DatabaseBatch> MakeBatch(
        bool = true) override
    {
        return std::make_unique<DummyBatch>();
    }
};

/** Recursively list database paths and formats in directory. */
std::vector<std::pair<fs::path, std::string>> ListDatabases(const fs::path& path);

void ReadDatabaseArgs(
    const ArgsManager& args,
    DatabaseOptions& options);

std::unique_ptr<WalletDatabase> MakeDatabase(
    const fs::path& path,
    const DatabaseOptions& options,
    DatabaseStatus& status,
    bilingual_str& error);

std::unique_ptr<WalletDatabase> CreateDummyWalletDatabase();

std::unique_ptr<WalletDatabase> CreateMockWalletDatabase();

fs::path BDBDataFile(const fs::path& path);
fs::path SQLiteDataFile(const fs::path& path);

bool IsBDBFile(const fs::path& path);
bool IsSQLiteFile(const fs::path& path);

} // namespace wallet

#endif // SCHILLINGCOIN_WALLET_DB_H
