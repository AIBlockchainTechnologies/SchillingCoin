// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_DB_H
#define SCHILLINGCOIN_WALLET_DB_H

#include "clientversion.h"
#include "fs.h"
#include "streams.h"

#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>

/**
 * Backend-independent database batch interface.
 *
 * Concrete database backends implement the serialized key/value operations,
 * transaction handling, and cursor handling. The public template functions
 * preserve SCH's existing wallet record serialization.
 */
class DatabaseBatch
{
private:
    virtual bool ReadKey(
        CDataStream&& key,
        CDataStream& value) = 0;

    virtual bool WriteKey(
        CDataStream&& key,
        CDataStream&& value,
        bool overwrite) = 0;

    virtual bool EraseKey(
        CDataStream&& key) = 0;

    virtual bool HasKey(
        CDataStream&& key) = 0;

public:
    DatabaseBatch() = default;
    virtual ~DatabaseBatch() = default;

    DatabaseBatch(const DatabaseBatch&) = delete;
    DatabaseBatch& operator=(const DatabaseBatch&) = delete;

    template <typename K, typename T>
    bool Read(const K& key, T& value)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);

        if (!ReadKey(std::move(ssKey), ssValue)) {
            return false;
        }

        try {
            ssValue >> value;
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
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;

        return WriteKey(
            std::move(ssKey),
            std::move(ssValue),
            overwrite);
    }

    template <typename K>
    bool Erase(const K& key)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        return EraseKey(std::move(ssKey));
    }

    template <typename K>
    bool Exists(const K& key)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        return HasKey(std::move(ssKey));
    }

    virtual void Flush() = 0;
    virtual void Close() = 0;

    virtual bool StartCursor() = 0;

    virtual bool ReadAtCursor(
        CDataStream& key,
        CDataStream& value,
        bool set_range,
        bool& complete) = 0;

    virtual void CloseCursor() = 0;

    virtual bool TxnBegin() = 0;
    virtual bool TxnCommit() = 0;
    virtual bool TxnAbort() = 0;

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

/**
 * Backend-independent wallet database interface.
 */
class WalletDatabase
{
public:
    WalletDatabase()
        : nUpdateCounter(0),
          nLastSeen(0),
          nLastFlushed(0),
          nLastWalletUpdate(0)
    {
    }

    virtual ~WalletDatabase() = default;

    WalletDatabase(const WalletDatabase&) = delete;
    WalletDatabase& operator=(const WalletDatabase&) = delete;

    virtual bool Rewrite(
        const char* skip = nullptr) = 0;

    virtual bool Backup(
        const std::string& destination) = 0;

    virtual void Flush(
        bool shutdown) = 0;

    virtual bool PeriodicFlush() = 0;

    virtual void IncrementUpdateCounter() = 0;
    virtual void ReloadDbEnv() = 0;

    virtual fs::path GetPathToFile() const = 0;

    virtual std::string Filename() const = 0;
    virtual std::string Format() const = 0;

    virtual std::unique_ptr<DatabaseBatch> MakeBatch(
        const char* mode = "r+",
        bool flush_on_close = true) = 0;

    std::atomic<unsigned int> nUpdateCounter;
    unsigned int nLastSeen;
    unsigned int nLastFlushed;
    int64_t nLastWalletUpdate;
};

/**
 * Database batch implementation with no persistent storage.
 */
class DummyBatch final : public DatabaseBatch
{
private:
    bool ReadKey(
        CDataStream&&,
        CDataStream&) override
    {
        return false;
    }

    bool WriteKey(
        CDataStream&&,
        CDataStream&&,
        bool) override
    {
        return true;
    }

    bool EraseKey(
        CDataStream&&) override
    {
        return true;
    }

    bool HasKey(
        CDataStream&&) override
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

    bool StartCursor() override
    {
        return true;
    }

    bool ReadAtCursor(
        CDataStream&,
        CDataStream&,
        bool,
        bool& complete) override
    {
        complete = true;
        return true;
    }

    void CloseCursor() override
    {
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
};

/**
 * Dummy wallet database used when persistent storage is unnecessary.
 */
class DummyDatabase final : public WalletDatabase
{
public:
    bool Rewrite(
        const char* = nullptr) override
    {
        return true;
    }

    bool Backup(
        const std::string&) override
    {
        return true;
    }

    void Flush(
        bool) override
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

    fs::path GetPathToFile() const override
    {
        return fs::path();
    }

    std::string Filename() const override
    {
        return "dummy";
    }

    std::string Format() const override
    {
        return "dummy";
    }

    std::unique_ptr<DatabaseBatch> MakeBatch(
        const char* = "r+",
        bool = true) override
    {
        return std::make_unique<DummyBatch>();
    }
};

enum class DatabaseStatus {
    SUCCESS,
    FAILED_BAD_PATH,
    FAILED_BAD_FORMAT,
    FAILED_ALREADY_LOADED,
    FAILED_ALREADY_EXISTS,
    FAILED_NOT_FOUND,
    FAILED_CREATE,
    FAILED_LOAD,
    FAILED_VERIFY
};

struct DatabaseOptions {
    bool require_existing{false};
    bool require_create{false};
    bool verify{true};
};

std::unique_ptr<WalletDatabase> MakeDatabase(
    const fs::path& path,
    const DatabaseOptions& options,
    DatabaseStatus& status,
    std::string& error);

std::unique_ptr<WalletDatabase> CreateDummyWalletDatabase();

std::unique_ptr<WalletDatabase> CreateMockWalletDatabase();

#endif // SCHILLINGCOIN_WALLET_DB_H