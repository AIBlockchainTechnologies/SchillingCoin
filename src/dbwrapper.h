// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_DBWRAPPER_H
#define SCHILLINGCOIN_DBWRAPPER_H

#include "clientversion.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "util/fs.h"
#include "util/system.h"
#include "version.h"

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

static const size_t DBWRAPPER_PREALLOC_KEY_SIZE = 64;
static const size_t DBWRAPPER_PREALLOC_VALUE_SIZE = 1024;

inline const char* CharCast(const std::byte* data)
{
    return reinterpret_cast<const char*>(data);
}

class CDBWrapper;

class dbwrapper_error : public std::runtime_error

{
public:
    dbwrapper_error(const std::string& msg) : std::runtime_error(msg) {}
};

namespace dbwrapper_private {

/** Handle database error by throwing dbwrapper_error exception. */
void HandleError(const leveldb::Status& status);

};

/** Batch of changes queued to be written to a CDBWrapper */
class CDBBatch
{
    friend class CDBWrapper;

private:
    const CDBWrapper& parent;
    leveldb::WriteBatch batch;

    DataStream ssKey{};
    CDataStream ssValue;

    size_t size_estimate{0};

public:
    /**
     * @param[in] _parent CDBWrapper that this batch is to be submitted to.
     */
    explicit CDBBatch(const CDBWrapper& _parent)
        : parent(_parent),
          ssValue(SER_DISK, CLIENT_VERSION)
    {
    }

    void Clear()
    {
        batch.Clear();
        size_estimate = 0;
    }

    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        Write(ssKey, value);
        ssKey.clear();
    }

    template <typename V>
    void Write(const DataStream& serialized_key, const V& value)
    {
        leveldb::Slice slKey(CharCast(serialized_key.data()), serialized_key.size());

        ssValue.reserve(DBWRAPPER_PREALLOC_VALUE_SIZE);
        ssValue << value;
        leveldb::Slice slValue(CharCast(ssValue.data()), ssValue.size());

        batch.Put(slKey, slValue);

        // LevelDB serializes writes as:
        // - byte: header
        // - varint: key length
        // - byte[\]: key
        // - varint: value length
        // - byte[\]: value
        // The formula below assumes the key and value are both less than 16k.
        size_estimate += 3 +
                         (slKey.size() > 127) +
                         slKey.size() +
                         (slValue.size() > 127) +
                         slValue.size();

        ssValue.clear();
    }

    template <typename K>
    void Erase(const K& key)
    {
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        Erase(ssKey);
        ssKey.clear();
    }

    void Erase(const DataStream& serialized_key)
    {
        leveldb::Slice slKey(CharCast(serialized_key.data()), serialized_key.size());

        batch.Delete(slKey);

        // LevelDB serializes erases as:
        // - byte: header
        // - varint: key length
        // - byte[\]: key
        // The formula below assumes the key is less than 16kB.
        size_estimate += 2 +
                         (slKey.size() > 127) +
                         slKey.size();
    }

    size_t SizeEstimate() const
    {
        return size_estimate;
    }
};

class CDBIterator
{
private:
    const CDBWrapper& parent;
    leveldb::Iterator* piter;

public:
    /**
     * @param[in] _parent Parent CDBWrapper instance.
     * @param[in] _piter  Original LevelDB iterator.
     */
    CDBIterator(const CDBWrapper& _parent, leveldb::Iterator* _piter)
        : parent(_parent),
          piter(_piter)
    {
    }

    ~CDBIterator();

    bool Valid() const;

    void SeekToFirst();

    template <typename K>
    void Seek(const K& key)
    {
        DataStream ssKey{};
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        Seek(ssKey);
    }

    void Seek(const DataStream& ssKey)
    {
        leveldb::Slice slKey(CharCast(ssKey.data()), ssKey.size());
        piter->Seek(slKey);
    }

    void Next();

    template <typename K>
    bool GetKey(K& key)
    {
        try {
            DataStream ssKey = GetKey();
            ssKey >> key;
        } catch (const std::exception&) {
            return false;
        }

        return true;
    }

    DataStream GetKey()
    {
        const leveldb::Slice slKey = piter->key();
        return DataStream{MakeByteSpan(slKey)};
    }

    unsigned int GetKeySize()
    {
        return piter->key().size();
    }

    template <typename V>
    bool GetValue(V& value)
    {
        const leveldb::Slice slValue = piter->value();

        try {
            CDataStream ssValue{MakeByteSpan(slValue), SER_DISK, CLIENT_VERSION};
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }

        return true;
    }

    unsigned int GetValueSize()
    {
        return piter->value().size();
    }
};

class CDBWrapper
{
private:
    //! custom environment this database is using (may be NULL in case of default environment)
    leveldb::Env* penv;

    //! database options used
    leveldb::Options options;

    //! options used when reading from the database
    leveldb::ReadOptions readoptions;

    //! options used when iterating over values of the database
    leveldb::ReadOptions iteroptions;

    //! options used when writing to the database
    leveldb::WriteOptions writeoptions;

    //! options used when sync writing to the database
    leveldb::WriteOptions syncoptions;

    //! the database itself
    leveldb::DB* pdb;

public:
    CDBWrapper(const fs::path& path, size_t nCacheSize, bool fMemory = false, bool fWipe = false);
    ~CDBWrapper();

    CDBWrapper(const CDBWrapper&) = delete;
    CDBWrapper& operator=(const CDBWrapper&) = delete;

    template <typename K>
    bool ReadDataStream(const K& key, CDataStream& ssValue) const
    {
        DataStream ssKey{};
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return ReadDataStream(ssKey, ssValue);
    }

    bool ReadDataStream(const DataStream& ssKey, CDataStream& ssValue) const
    {
        leveldb::Slice slKey(CharCast(ssKey.data()), ssKey.size());

        std::string strValue;
        const leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);

        if (!status.ok()) {
            if (status.IsNotFound()) {
                return false;
            }

            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            dbwrapper_private::HandleError(status);
        }

        ssValue = CDataStream{MakeByteSpan(strValue), SER_DISK, CLIENT_VERSION};
        return true;
    }

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        DataStream ssKey{};
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return Read(ssKey, value);
    }

    template <typename V>
    bool Read(const DataStream& ssKey, V& value) const
    {
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);

        if (!ReadDataStream(ssKey, ssValue)) {
            return false;
        }

        try {
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }

        return true;
    }

    template <typename K, typename V>
    bool Write(const K& key, const V& value, bool fSync = false)
    {
        CDBBatch batch(*this);
        batch.Write(key, value);
        return WriteBatch(batch, fSync);
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        DataStream ssKey{};
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return Exists(ssKey);
    }

    bool Exists(const DataStream& key) const
    {
        leveldb::Slice slKey(CharCast(key.data()), key.size());

        std::string strValue;
        const leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);

        if (!status.ok()) {
            if (status.IsNotFound()) {
                return false;
            }

            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            dbwrapper_private::HandleError(status);
        }

        return true;
    }

    template <typename K>
    bool Erase(const K& key, bool fSync = false)
    {
        CDBBatch batch(*this);
        batch.Erase(key);
        return WriteBatch(batch, fSync);
    }

    bool WriteBatch(CDBBatch& batch, bool fSync = false);

    CDBIterator* NewIterator()
    {
        return new CDBIterator(*this, pdb->NewIterator(iteroptions));
    }

    /**
     * Return true if the database managed by this class contains no entries.
     */
    bool IsEmpty();

    template <typename K>
    size_t EstimateSize(const K& key_begin, const K& key_end) const
    {
        DataStream ssKey1{};
        DataStream ssKey2{};

        ssKey1.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey2.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);

        ssKey1 << key_begin;
        ssKey2 << key_end;

        leveldb::Slice slKey1(CharCast(ssKey1.data()), ssKey1.size());
        leveldb::Slice slKey2(CharCast(ssKey2.data()), ssKey2.size());

        uint64_t size{0};
        leveldb::Range range(slKey1, slKey2);
        pdb->GetApproximateSizes(&range, 1, &size);
        return size;
    }
};

#endif // SCHILLINGCOIN_DBWRAPPER_H