// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/bdb.h"

#include "addrman.h"
#include "guiinterfaceutil.h"
#include "hash.h"
#include "logging.h"
#include "protocol.h"
#include "util/system.h"
#include "util/string.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "wallet/walletutil.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <sstream>
#include <tuple>
#include <utility>

#ifndef WIN32
#include <sys/stat.h>
#endif

#include <boost/thread.hpp>
#include <boost/version.hpp>

namespace
{

/**
 * Make sure the database has a unique file ID within the environment.
 *
 * BerkeleyDB caches do not work correctly when more than one open database
 * has the same file ID. Values written to one database can otherwise appear
 * in reads from another database.
 */
void CheckUniqueFileid(const BerkeleyEnvironment& env, const std::string& filename, Db& db, WalletDatabaseFileId& fileid)
{
    if (env.IsMock())
        return;

    const int ret = db.get_mpf()->get_fileid(fileid.value);
    if (ret != 0) {
        throw std::runtime_error(strprintf(
            "BerkeleyBatch: Can't open database %s "
            "(get_fileid failed with %d)",
            filename,
            ret));
    }

    for (const auto& item : env.m_fileids) {
        if (fileid == item.second && &fileid != &item.second) {
            throw std::runtime_error(strprintf(
                "BerkeleyBatch: Can't open database %s "
                "(duplicates fileid %s from %s)",
                filename,
                HexStr(std::begin(item.second.value), std::end(item.second.value)),
                item.first));
        }
    }
}

RecursiveMutex cs_db;

/** Map from environment directory name to Berkeley database environment. */
std::map<std::string, BerkeleyEnvironment> g_dbenvs;

} // namespace

bool WalletDatabaseFileId::operator==(const WalletDatabaseFileId& rhs) const
{
    return std::memcmp(value, rhs.value, sizeof(value)) == 0;
}

fs::path BDBDataFile(const fs::path& wallet_path)
{
    if (fs::is_regular_file(wallet_path)) {
        // Backwards compatibility with legacy wallet.dat files.
        return wallet_path;
    }

    // Modern wallet-directory layout.
    return wallet_path / "wallet.dat";
}

BerkeleyEnvironment* GetWalletEnv(const fs::path& wallet_path, std::string& database_filename)
{
    const fs::path data_file = BDBDataFile(wallet_path);
    const fs::path env_directory = data_file.parent_path();

    database_filename = data_file.filename().string();

    LOCK(cs_db);

    return &g_dbenvs.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(env_directory.string()),
        std::forward_as_tuple(env_directory)
    ).first->second;
}

void BerkeleyEnvironment::Close()
{
    if (!fDbEnvInit)
        return;

    fDbEnvInit = false;

    for (auto& db : mapDb) {
        auto count = mapFileUseCount.find(db.first);
        assert(count == mapFileUseCount.end() || count->second == 0);

        if (db.second) {
            db.second->close(0);
            delete db.second;
            db.second = nullptr;
        }
    }

    int ret = dbenv->close(0);
    if (ret != 0) {
        LogPrintf("%s: Error %d closing database environment: %s\n",
            __func__, ret, DbEnv::strerror(ret));
    }

    if (!fMockDb) {
        DbEnv(static_cast<u_int32_t>(0)).remove(strPath.c_str(), 0);
    }
}

void BerkeleyEnvironment::Reset()
{
    dbenv.reset(new DbEnv(DB_CXX_NO_EXCEPTIONS));
    fDbEnvInit = false;
    fMockDb = false;
}

BerkeleyEnvironment::BerkeleyEnvironment(const fs::path& dir_path) :
    strPath(dir_path.string())
{
    Reset();
}

BerkeleyEnvironment::~BerkeleyEnvironment()
{
    Close();
}

bool BerkeleyEnvironment::Open(bool retry)
{
    if (fDbEnvInit)
        return true;

    boost::this_thread::interruption_point();

    fs::path pathIn = strPath;
    TryCreateDirectories(pathIn);

    if (!LockDirectory(pathIn, ".walletlock")) {
        LogPrintf(
            "Cannot obtain a lock on wallet directory %s. "
            "Another instance of SchillingCoin may be using it.\n",
            strPath);
        return false;
    }

    fs::path pathLogDir = pathIn / "database";
    TryCreateDirectories(pathLogDir);

    fs::path pathErrorFile = pathIn / "db.log";

    LogPrintf(
        "BerkeleyEnvironment::Open: LogDir=%s ErrorFile=%s\n",
        pathLogDir.string(),
        pathErrorFile.string());

    unsigned int nEnvFlags = 0;
    if (gArgs.GetBoolArg("-privdb", DEFAULT_WALLET_PRIVDB))
        nEnvFlags |= DB_PRIVATE;

    dbenv->set_lg_dir(pathLogDir.string().c_str());
    dbenv->set_cachesize(0, 0x100000, 1);
    dbenv->set_lg_bsize(0x10000);
    dbenv->set_lg_max(1048576);
    dbenv->set_lk_max_locks(40000);
    dbenv->set_lk_max_objects(40000);
    dbenv->set_errfile(fsbridge::fopen(pathErrorFile, "a"));
    dbenv->set_flags(DB_AUTO_COMMIT, 1);
    dbenv->set_flags(DB_TXN_WRITE_NOSYNC, 1);
    dbenv->log_set_config(DB_LOG_AUTO_REMOVE, 1);

    int ret = dbenv->open(
        strPath.c_str(),
        DB_CREATE |
            DB_INIT_LOCK |
            DB_INIT_LOG |
            DB_INIT_MPOOL |
            DB_INIT_TXN |
            DB_THREAD |
            DB_RECOVER |
            nEnvFlags,
        S_IRUSR | S_IWUSR);

    if (ret != 0) {
        LogPrintf(
            "%s: Error %d opening database environment: %s\n",
            __func__,
            ret,
            DbEnv::strerror(ret));

        const int closeRet = dbenv->close(0);
        if (closeRet != 0) {
            LogPrintf(
                "%s: Error %d closing failed database environment: %s\n",
                __func__,
                closeRet,
                DbEnv::strerror(closeRet));
        }

        Reset();

        if (!retry)
            return false;

        fs::path pathDatabaseBak =
            pathIn / strprintf("database.%d.bak", GetTime());

        try {
            fs::rename(pathLogDir, pathDatabaseBak);

            LogPrintf(
                "Moved old %s to %s. Retrying.\n",
                pathLogDir.string(),
                pathDatabaseBak.string());
        } catch (const fs::filesystem_error&) {
            // A failed move does not make the retry worse than the original state.
        }

        if (!Open(false /* retry */))
            return false;
    }

    fDbEnvInit = true;
    fMockDb = false;
    return true;
}

void BerkeleyEnvironment::MakeMock()
{
    if (fDbEnvInit) {
        throw std::runtime_error(
            "BerkeleyEnvironment::MakeMock : Already initialized");
    }

    boost::this_thread::interruption_point();

    LogPrint(BCLog::DB, "BerkeleyEnvironment::MakeMock\n");

    dbenv->set_cachesize(1, 0, 1);
    dbenv->set_lg_bsize(10485760 * 4);
    dbenv->set_lg_max(10485760);
    dbenv->set_lk_max_locks(10000);
    dbenv->set_lk_max_objects(10000);
    dbenv->set_flags(DB_AUTO_COMMIT, 1);
    dbenv->log_set_config(DB_LOG_IN_MEMORY, 1);

    int ret = dbenv->open(
        nullptr,
        DB_CREATE |
            DB_INIT_LOCK |
            DB_INIT_LOG |
            DB_INIT_MPOOL |
            DB_INIT_TXN |
            DB_THREAD |
            DB_PRIVATE,
        S_IRUSR | S_IWUSR);

    if (ret > 0) {
        throw std::runtime_error(strprintf(
            "BerkeleyEnvironment::MakeMock : Error %d opening database environment.",
            ret));
    }

    fDbEnvInit = true;
    fMockDb = true;
}

BerkeleyEnvironment::VerifyResult BerkeleyEnvironment::Verify(const std::string& strFile, recoverFunc_type recoverFunc, std::string& out_backup_filename)
{
    LOCK(cs_db);
    assert(mapFileUseCount.count(strFile) == 0);

    Db db(dbenv.get(), 0);
    int result = db.verify(strFile.c_str(), nullptr, nullptr, 0);

    if (result == 0) {
        return VERIFY_OK;
    }

    if (recoverFunc == nullptr) {
        return RECOVER_FAIL;
    }

    bool fRecovered = (*recoverFunc)(
        fs::path(strPath) / strFile,
        out_backup_filename);

    return fRecovered ? RECOVER_OK : RECOVER_FAIL;
}

bool BerkeleyBatch::Recover(const fs::path& file_path, void* callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& newFilename)
{
    std::string filename;
    BerkeleyEnvironment* env = GetWalletEnv(file_path, filename);

    // Recovery procedure:
    // Move wallet file to walletfilename.timestamp.bak.
    // Call Salvage with fAggressive=true to get as much data as possible.
    // Rewrite salvaged data to a fresh wallet file.
    const int64_t now = GetTime();
    newFilename = strprintf("%s.%d.bak", filename, now);

    const int result = env->dbenv->dbrename(
        nullptr,
        filename.c_str(),
        nullptr,
        newFilename.c_str(),
        DB_AUTO_COMMIT);

    if (result == 0) {
        LogPrintf("Renamed %s to %s\n", filename, newFilename);
    } else {
        LogPrintf("Failed to rename %s to %s\n", filename, newFilename);
        return false;
    }

    std::vector<BerkeleyEnvironment::KeyValPair> salvagedData;
    bool fSuccess = env->Salvage(newFilename, true, salvagedData);

    if (salvagedData.empty()) {
        LogPrintf("Salvage(aggressive) found no records in %s.\n", newFilename);
        return false;
    }

    LogPrintf("Salvage(aggressive) found %u records\n", salvagedData.size());

    std::unique_ptr<Db> pdbCopy =
        std::make_unique<Db>(env->dbenv.get(), 0);

    const int ret = pdbCopy->open(
        nullptr,
        filename.c_str(),
        "main",
        DB_BTREE,
        DB_CREATE,
        0);

    if (ret > 0) {
        LogPrintf("Cannot create database file %s\n", filename);
        pdbCopy->close(0);
        return false;
    }

    DbTxn* ptxn = env->TxnBegin();
    if (!ptxn) {
        pdbCopy->close(0);
        return false;
    }

    for (BerkeleyEnvironment::KeyValPair& row : salvagedData) {
        if (recoverKVcallback) {
            CDataStream ssKey(
                row.first,
                SER_DISK,
                CLIENT_VERSION);

            CDataStream ssValue(
                row.second,
                SER_DISK,
                CLIENT_VERSION);

            if (!(*recoverKVcallback)(
                    callbackDataIn,
                    ssKey,
                    ssValue)) {
                continue;
            }
        }

        Dbt datKey(row.first.data(), row.first.size());
        Dbt datValue(row.second.data(), row.second.size());

        const int ret2 = pdbCopy->put(
            ptxn,
            &datKey,
            &datValue,
            DB_NOOVERWRITE);

        if (ret2 > 0)
            fSuccess = false;
    }

    if (ptxn->commit(0) != 0)
        fSuccess = false;

    if (pdbCopy->close(0) != 0)
        fSuccess = false;

    return fSuccess;
}

bool BerkeleyBatch::VerifyEnvironment(const fs::path& file_path, std::string& errorStr)
{
    std::string walletFile;
    BerkeleyEnvironment* env = GetWalletEnv(file_path, walletFile);
    const fs::path walletDir = env->Directory();

    LogPrintf("Using BerkeleyDB version %s\n", DbEnv::version(0, 0, 0));
    LogPrintf("Using wallet %s\n", walletFile);

    // Wallet file must be a plain filename without a directory.
    const fs::path walletFilePath(walletFile);
    if (walletFile != walletFilePath.filename().string()) {
        return UIError(strprintf(
            _("Wallet %s resides outside wallet directory %s"),
            walletFile,
            walletDir.string()));
    }

    if (!env->Open(true /* retry */)) {
        errorStr = strprintf(
            _("Error initializing wallet database environment %s!"),
            walletDir.string());

        return false;
    }

    return true;
}

bool BerkeleyBatch::VerifyDatabaseFile(const fs::path& file_path, std::string& warningStr, std::string& errorStr, BerkeleyEnvironment::recoverFunc_type recoverFunc)
{
    std::string walletFile;
    BerkeleyEnvironment* env = GetWalletEnv(file_path, walletFile);
    const fs::path walletDir = env->Directory();

    if (fs::exists(walletDir / walletFile)) {
        std::string backupFilename;

        const BerkeleyEnvironment::VerifyResult result =
            env->Verify(
                walletFile,
                recoverFunc,
                backupFilename);

        if (result == BerkeleyEnvironment::RECOVER_OK) {
            warningStr = strprintf(
                _("Warning: Wallet file corrupt, data salvaged! "
                  "Original %s saved as %s in %s; if your balance "
                  "or transactions are incorrect, restore from a backup."),
                walletFile,
                backupFilename,
                walletDir.string());
        }

        if (result == BerkeleyEnvironment::RECOVER_FAIL) {
            errorStr = strprintf(
                _("%s corrupt, salvage failed"),
                walletFile);

            return false;
        }
    }

    // Also return true if the wallet file does not yet exist.
    return true;
}

/* End of headers, beginning of key/value data */
static const char* HEADER_END = "HEADER=END";

/* End of key/value data */
static const char* DATA_END = "DATA=END";

bool BerkeleyEnvironment::Salvage(const std::string& strFile, bool fAggressive, std::vector<BerkeleyEnvironment::KeyValPair>& vResult)
{
    LOCK(cs_db);

    assert(mapFileUseCount.count(strFile) == 0);

    u_int32_t flags = DB_SALVAGE;
    if (fAggressive)
        flags |= DB_AGGRESSIVE;

    std::stringstream strDump;

    Db db(dbenv.get(), 0);
    const int result = db.verify(
        strFile.c_str(),
        nullptr,
        &strDump,
        flags);

    if (result == DB_VERIFY_BAD) {
        LogPrintf(
            "BerkeleyEnvironment::Salvage: Database salvage found "
            "errors, all data may not be recoverable.\n");

        if (!fAggressive) {
            LogPrintf(
                "BerkeleyEnvironment::Salvage: Rerun with aggressive "
                "mode to ignore errors and continue.\n");

            return false;
        }
    }

    if (result != 0 && result != DB_VERIFY_BAD) {
        LogPrintf(
            "BerkeleyEnvironment::Salvage: Database salvage failed "
            "with result %d.\n",
            result);

        return false;
    }

    // Format of BerkeleyDB dump:
    // Header lines
    // HEADER=END
    // Hexadecimal key
    // Hexadecimal value
    // Repeated key/value pairs
    // DATA=END
    std::string strLine;

    while (!strDump.eof() && strLine != HEADER_END)
        std::getline(strDump, strLine);

    std::string keyHex;
    std::string valueHex;

    while (!strDump.eof() && keyHex != DATA_END) {
        std::getline(strDump, keyHex);

        if (keyHex == DATA_END)
            break;

        if (strDump.eof())
            break;

        std::getline(strDump, valueHex);

        if (valueHex == DATA_END) {
            LogPrintf(
                "BerkeleyEnvironment::Salvage: WARNING: Number of "
                "keys does not match number of values.\n");

            break;
        }

        vResult.emplace_back(
            ParseHex(keyHex),
            ParseHex(valueHex));
    }

    if (keyHex != DATA_END) {
        LogPrintf(
            "BerkeleyEnvironment::Salvage: WARNING: Unexpected end "
            "of file while reading salvage output.\n");

        return false;
    }

    return result == 0;
}

void BerkeleyEnvironment::CheckpointLSN(const std::string& strFile)
{
    (void)strFile;
    dbenv->txn_checkpoint(0, 0, 0);
}

void BerkeleyEnvironment::ResetLSN(const std::string& strFile)
{
    if (!fMockDb)
        dbenv->lsn_reset(strFile.c_str(), 0);
}

BerkeleyBatch::BerkeleyBatch(BerkeleyDatabase& database, const char* pszMode, bool fFlushOnCloseIn) : pdb(nullptr), activeTxn(nullptr), fReadOnly(!strchr(pszMode, '+') && !strchr(pszMode, 'w')), fFlushOnClose(fFlushOnCloseIn), env(database.env), m_cursor(nullptr)
{
    if (database.IsDummy()) {
        return;
    }

    const std::string& strFilename = database.strFile;
    const bool fCreate = strchr(pszMode, 'c') != nullptr;

    unsigned int nFlags = DB_THREAD;
    if (fCreate) {
        nFlags |= DB_CREATE;
    }

    {
        LOCK(cs_db);

        if (!env->Open(false /* retry */)) {
            throw std::runtime_error(
                "BerkeleyBatch: Failed to open database environment.");
        }

        pdb = env->mapDb[strFilename];

        if (pdb == nullptr) {
            std::unique_ptr<Db> pdbTemp =
                std::make_unique<Db>(env->dbenv.get(), 0);

            const bool fMockDb = env->IsMock();

            if (fMockDb) {
                DbMpoolFile* mpf = pdbTemp->get_mpf();
                const int ret = mpf->set_flags(DB_MPOOL_NOFILE, 1);

                if (ret != 0) {
                    throw std::runtime_error(strprintf(
                        "BerkeleyBatch: Failed to configure no-file "
                        "backing for database %s",
                        strFilename));
                }
            }

            const int ret = pdbTemp->open(
                nullptr,
                fMockDb ? nullptr : strFilename.c_str(),
                fMockDb ? strFilename.c_str() : "main",
                DB_BTREE,
                nFlags,
                0);

            if (ret != 0) {
                throw std::runtime_error(strprintf(
                    "BerkeleyBatch: Error %d, cannot open database %s",
                    ret,
                    strFilename));
            }

            // Check every known environment to prevent the same BerkeleyDB
            // file from being opened through an equivalent hard-linked,
            // symlinked, or bind-mounted path.
            for (const auto& environment : g_dbenvs) {
                CheckUniqueFileid(
                    environment.second,
                    strFilename,
                    *pdbTemp,
                    env->m_fileids[strFilename]);
            }

            pdb = pdbTemp.release();
            env->mapDb[strFilename] = pdb;

            if (fCreate && !Exists(std::string("version"))) {
                const bool wasReadOnly = fReadOnly;
                fReadOnly = false;
                WriteVersion(CLIENT_VERSION);
                fReadOnly = wasReadOnly;
            }
        }

        ++env->mapFileUseCount[strFilename];
        strFile = strFilename;
    }
}

bool BerkeleyBatch::ReadKey(CDataStream&& key, CDataStream& value)
{
    if (!pdb) {
        return false;
    }

    Dbt datKey(key.data(), key.size());

    Dbt datValue;
    datValue.set_flags(DB_DBT_MALLOC);

    const int ret = pdb->get(
        activeTxn,
        &datKey,
        &datValue,
        0);

    memory_cleanse(
        datKey.get_data(),
        datKey.get_size());

    if (ret != 0 || datValue.get_data() == nullptr) {
        if (datValue.get_data() != nullptr) {
            memory_cleanse(
                datValue.get_data(),
                datValue.get_size());

            free(datValue.get_data());
        }

        return false;
    }

    value.SetType(SER_DISK);
    value.clear();
    value.write(
        static_cast<char*>(datValue.get_data()),
        datValue.get_size());

    memory_cleanse(
        datValue.get_data(),
        datValue.get_size());

    free(datValue.get_data());

    return true;
}

bool BerkeleyBatch::WriteKey(CDataStream&& key, CDataStream&& value, bool overwrite)
{
    if (!pdb) {
        return true;
    }

    if (fReadOnly) {
        assert(!"WriteKey called on database in read-only mode");
    }

    Dbt datKey(key.data(), key.size());
    Dbt datValue(value.data(), value.size());

    const int ret = pdb->put(
        activeTxn,
        &datKey,
        &datValue,
        overwrite ? 0 : DB_NOOVERWRITE);

    memory_cleanse(
        datKey.get_data(),
        datKey.get_size());

    memory_cleanse(
        datValue.get_data(),
        datValue.get_size());

    return ret == 0;
}

bool BerkeleyBatch::EraseKey(CDataStream&& key)
{
    if (!pdb) {
        return false;
    }

    if (fReadOnly) {
        assert(!"EraseKey called on database in read-only mode");
    }

    Dbt datKey(key.data(), key.size());

    const int ret = pdb->del(
        activeTxn,
        &datKey,
        0);

    memory_cleanse(
        datKey.get_data(),
        datKey.get_size());

    return ret == 0 || ret == DB_NOTFOUND;
}

bool BerkeleyBatch::HasKey(CDataStream&& key)
{
    if (!pdb) {
        return false;
    }

    Dbt datKey(key.data(), key.size());

    const int ret = pdb->exists(
        activeTxn,
        &datKey,
        0);

    memory_cleanse(
        datKey.get_data(),
        datKey.get_size());

    return ret == 0;
}

bool BerkeleyBatch::StartCursor()
{
    if (!pdb || m_cursor != nullptr) {
        return false;
    }

    return pdb->cursor(
        nullptr,
        &m_cursor,
        0) == 0;
}

bool BerkeleyBatch::ReadAtCursor(CDataStream& ssKey, CDataStream& ssValue, bool setRange, bool& complete)
{
    complete = false;

    if (!m_cursor) {
        return false;
    }

    Dbt datKey;
    unsigned int flags = DB_NEXT;

    if (setRange) {
        datKey.set_data(ssKey.data());
        datKey.set_size(ssKey.size());
        flags = DB_SET_RANGE;
    }

    Dbt datValue;

    datKey.set_flags(DB_DBT_MALLOC);
    datValue.set_flags(DB_DBT_MALLOC);

    const int ret = m_cursor->get(
        &datKey,
        &datValue,
        flags);

    if (ret == DB_NOTFOUND) {
        complete = true;
        return true;
    }

    if (ret != 0) {
        return false;
    }

    if (datKey.get_data() == nullptr ||
        datValue.get_data() == nullptr) {
        return false;
    }

    ssKey.SetType(SER_DISK);
    ssKey.clear();
    ssKey.write(
        static_cast<char*>(datKey.get_data()),
        datKey.get_size());

    ssValue.SetType(SER_DISK);
    ssValue.clear();
    ssValue.write(
        static_cast<char*>(datValue.get_data()),
        datValue.get_size());

    memory_cleanse(
        datKey.get_data(),
        datKey.get_size());

    memory_cleanse(
        datValue.get_data(),
        datValue.get_size());

    free(datKey.get_data());
    free(datValue.get_data());

    return true;
}

void BerkeleyBatch::CloseCursor()
{
    if (!m_cursor) {
        return;
    }

    m_cursor->close();
    m_cursor = nullptr;
}

void BerkeleyBatch::Flush()
{
    if (activeTxn)
        return;

    // Flush database activity from the memory pool to the disk log.
    unsigned int nMinutes = 0;

    if (fReadOnly)
        nMinutes = 1;

    env->dbenv->txn_checkpoint(
        nMinutes
            ? gArgs.GetArg(
                  "-dblogsize",
                  DEFAULT_WALLET_DBLOGSIZE) *
                  1024
            : 0,
        nMinutes,
        0);
}

void BerkeleyDatabase::IncrementUpdateCounter()
{
    ++nUpdateCounter;
}

void BerkeleyBatch::Close()
{
    CloseCursor();

    if (!pdb) {
        return;
    }

    if (activeTxn) {
        activeTxn->abort();
    }

    activeTxn = nullptr;

    if (fFlushOnClose) {
        Flush();
    }

    pdb = nullptr;

    {
        LOCK(cs_db);
        --env->mapFileUseCount[strFile];
    }

    env->m_db_in_use.notify_all();
}

void BerkeleyEnvironment::CloseDb(const std::string& strFile)
{
    LOCK(cs_db);

    const auto database = mapDb.find(strFile);

    if (database == mapDb.end() || database->second == nullptr)
        return;

    database->second->close(0);
    delete database->second;
    database->second = nullptr;
}

void BerkeleyEnvironment::ReloadDbEnv()
{
    AssertLockNotHeld(cs_db);

    std::unique_lock<RecursiveMutex> lock(cs_db);

    m_db_in_use.wait(lock, [this]() {
        for (const auto& count : mapFileUseCount) {
            if (count.second > 0)
                return false;
        }

        return true;
    });

    std::vector<std::string> filenames;
    filenames.reserve(mapDb.size());

    for (const auto& database : mapDb)
        filenames.push_back(database.first);

    for (const std::string& filename : filenames)
        CloseDb(filename);

    // Flush and close the environment, then recreate and reopen it.
    Flush(true);
    Reset();
    Open(true /* retry */);
}

bool BerkeleyBatch::Rewrite(BerkeleyDatabase& database, const char* pszSkip)
{
    if (database.IsDummy())
        return true;

    BerkeleyEnvironment* env = database.env;
    const std::string& strFile = database.strFile;

    while (true) {
        {
            LOCK(cs_db);

            if (!env->mapFileUseCount.count(strFile) ||
                env->mapFileUseCount[strFile] == 0) {
                // Flush log data to the database file.
                env->CloseDb(strFile);
                env->CheckpointLSN(strFile);
                env->mapFileUseCount.erase(strFile);

                bool fSuccess = true;

                LogPrintf(
                    "BerkeleyBatch::Rewrite: Rewriting %s...\n",
                    strFile);

                const std::string strFileRes =
                    strFile + ".rewrite";

                {
                    BerkeleyBatch batch(database, "r");

                    std::unique_ptr<Db> pdbCopy =
                        std::make_unique<Db>(
                            env->dbenv.get(),
                            0);

                    const int openResult = pdbCopy->open(
                        nullptr,
                        strFileRes.c_str(),
                        "main",
                        DB_BTREE,
                        DB_CREATE,
                        0);

                    if (openResult > 0) {
                        LogPrintf(
                            "BerkeleyBatch::Rewrite: Cannot create "
                            "database file %s\n",
                            strFileRes);

                        fSuccess = false;
                    }

                    if (batch.StartCursor()) {
                        while (fSuccess) {
                            CDataStream ssKey(
                                SER_DISK,
                                CLIENT_VERSION);

                            CDataStream ssValue(
                                SER_DISK,
                                CLIENT_VERSION);

                            bool complete = false;

                            if (!batch.ReadAtCursor(
                                    ssKey,
                                    ssValue,
                                    false,
                                    complete)) {
                                fSuccess = false;
                                break;
                            }

                            if (complete) {
                                break;
                            }

                            if (pszSkip &&
                                strncmp(
                                    ssKey.data(),
                                    pszSkip,
                                    std::min(
                                        ssKey.size(),
                                        strlen(pszSkip))) == 0) {
                                continue;
                            }

                            if (strncmp(
                                    ssKey.data(),
                                    "\x07version",
                                    8) == 0) {
                                ssValue.clear();
                                ssValue << CLIENT_VERSION;
                            }

                            Dbt datKey(
                                ssKey.data(),
                                ssKey.size());

                            Dbt datValue(
                                ssValue.data(),
                                ssValue.size());

                            const int writeResult =
                                pdbCopy->put(
                                    nullptr,
                                    &datKey,
                                    &datValue,
                                    DB_NOOVERWRITE);

                            if (writeResult > 0) {
                                fSuccess = false;
                            }
                        }

                        batch.CloseCursor();
                    } else {
                        fSuccess = false;
                    }

                    if (fSuccess) {
                        batch.Close();
                        env->CloseDb(strFile);

                        if (pdbCopy->close(0) != 0)
                            fSuccess = false;
                    } else {
                        pdbCopy->close(0);
                    }
                }

                if (fSuccess) {
                    Db oldDatabase(env->dbenv.get(), 0);

                    if (oldDatabase.remove(
                            strFile.c_str(),
                            nullptr,
                            0) != 0) {
                        fSuccess = false;
                    }

                    Db replacementDatabase(
                        env->dbenv.get(),
                        0);

                    if (replacementDatabase.rename(
                            strFileRes.c_str(),
                            nullptr,
                            strFile.c_str(),
                            0) != 0) {
                        fSuccess = false;
                    }
                }

                if (!fSuccess) {
                    LogPrintf(
                        "BerkeleyBatch::Rewrite: Failed to rewrite "
                        "database file %s\n",
                        strFileRes);
                }

                return fSuccess;
            }
        }

        MilliSleep(100);
    }
}

void BerkeleyEnvironment::Flush(bool fShutdown)
{
    const int64_t nStart = GetTimeMillis();

    LogPrint(
        BCLog::DB,
        "BerkeleyEnvironment::Flush: Flush(%s)%s\n",
        fShutdown ? "true" : "false",
        fDbEnvInit ? "" : " database not started");

    if (!fDbEnvInit)
        return;

    {
        LOCK(cs_db);

        auto fileUse = mapFileUseCount.begin();

        while (fileUse != mapFileUseCount.end()) {
            const std::string strFile = fileUse->first;
            const int nRefCount = fileUse->second;

            LogPrint(
                BCLog::DB,
                "BerkeleyEnvironment::Flush: Flushing %s "
                "(refcount = %d)...\n",
                strFile,
                nRefCount);

            if (nRefCount == 0) {
                CloseDb(strFile);

                LogPrint(
                    BCLog::DB,
                    "BerkeleyEnvironment::Flush: %s checkpoint\n",
                    strFile);

                CheckpointLSN(strFile);

                // SCH deliberately performs LSN reset only during final
                // shutdown. Ordinary flushes and periodic flushes checkpoint
                // without resetting the LSN.
                if (fShutdown) {
                    LogPrint(
                        BCLog::DB,
                        "BerkeleyEnvironment::Flush: %s reset LSN\n",
                        strFile);

                    ResetLSN(strFile);
                }

                LogPrint(
                    BCLog::DB,
                    "BerkeleyEnvironment::Flush: %s closed\n",
                    strFile);

                mapFileUseCount.erase(fileUse++);
            } else {
                ++fileUse;
            }
        }

        LogPrint(
            BCLog::DB,
            "BerkeleyEnvironment::Flush: Flush(%s)%s took %15dms\n",
            fShutdown ? "true" : "false",
            fDbEnvInit ? "" : " database not started",
            GetTimeMillis() - nStart);

        if (fShutdown && mapFileUseCount.empty()) {
            char** listp = nullptr;

            dbenv->log_archive(
                &listp,
                DB_ARCH_REMOVE);

            Close();

            if (!fMockDb) {
                fs::remove_all(
                    fs::path(strPath) / "database");
            }
        }
    }
}

bool BerkeleyBatch::PeriodicFlush(BerkeleyDatabase& database)
{
    if (database.IsDummy())
        return true;

    bool flushed = false;

    BerkeleyEnvironment* env = database.env;
    const std::string& strFile = database.strFile;

    TRY_LOCK(cs_db, lockDb);

    if (lockDb) {
        int nRefCount = 0;

        for (const auto& fileUse : env->mapFileUseCount)
            nRefCount += fileUse.second;

        if (nRefCount == 0) {
            boost::this_thread::interruption_point();

            const auto fileUse =
                env->mapFileUseCount.find(strFile);

            if (fileUse != env->mapFileUseCount.end()) {
                LogPrint(
                    BCLog::DB,
                    "Flushing %s\n",
                    strFile);

                const int64_t nStart =
                    GetTimeMillis();

                env->CloseDb(strFile);
                env->CheckpointLSN(strFile);

                // Periodic flushing must not reset the LSN.
                env->mapFileUseCount.erase(fileUse);

                LogPrint(
                    BCLog::DB,
                    "Flushed %s %dms\n",
                    strFile,
                    GetTimeMillis() - nStart);

                flushed = true;
            }
        }
    }

    return flushed;
}

bool BerkeleyDatabase::PeriodicFlush()
{
    return BerkeleyBatch::PeriodicFlush(*this);
}

std::unique_ptr<DatabaseBatch> BerkeleyDatabase::MakeBatch(
    const char* mode,
    bool flush_on_close)
{
    return std::make_unique<BerkeleyBatch>(
        *this,
        mode,
        flush_on_close);
}

bool BerkeleyDatabase::Rewrite(const char* pszSkip)
{
    return BerkeleyBatch::Rewrite(
        *this,
        pszSkip);
}

bool BerkeleyDatabase::Backup(const std::string& strDest)
{
    if (IsDummy())
        return false;

    while (true) {
        {
            LOCK(cs_db);

            if (!env->mapFileUseCount.count(strFile) ||
                env->mapFileUseCount[strFile] == 0) {
                env->CloseDb(strFile);
                env->CheckpointLSN(strFile);

                // Explicit reset makes the copied wallet database portable.
                env->ResetLSN(strFile);

                env->mapFileUseCount.erase(strFile);

                const fs::path pathSrc =
                    env->Directory() / strFile;

                fs::path pathDest(strDest);

                if (fs::is_directory(pathDest))
                    pathDest /= strFile;

                try {
                    if (fs::exists(pathDest) &&
                        fs::equivalent(pathSrc, pathDest)) {
                        LogPrintf(
                            "Cannot back up wallet to source file %s\n",
                            pathDest.string());

                        return false;
                    }

#if BOOST_VERSION >= 107400
                    fs::copy_file(
                        pathSrc,
                        pathDest,
                        fs::copy_options::overwrite_existing);
#elif BOOST_VERSION >= 105800
                    fs::copy_file(
                        pathSrc,
                        pathDest,
                        fs::copy_option::overwrite_if_exists);
#else
                    if (fs::exists(pathDest))
                        fs::remove(pathDest);

                    fs::copy_file(
                        pathSrc,
                        pathDest);
#endif

                    LogPrintf(
                        "Copied %s to %s\n",
                        strFile,
                        pathDest.string());

                    return true;
                } catch (const fs::filesystem_error& error) {
                    LogPrintf(
                        "Error copying %s to %s: %s\n",
                        strFile,
                        pathDest.string(),
                        fsbridge::get_filesystem_error_message(error));

                    return false;
                }
            }
        }

        MilliSleep(100);
    }
}

void BerkeleyDatabase::Flush(bool shutdown)
{
    if (IsDummy())
        return;

    env->Flush(shutdown);

    if (shutdown) {
        LOCK(cs_db);

        g_dbenvs.erase(
            env->Directory().string());

        env = nullptr;
    } else {
        env->m_fileids.erase(strFile);
    }
}

void BerkeleyDatabase::ReloadDbEnv()
{
    if (!IsDummy())
        env->ReloadDbEnv();
}

std::unique_ptr<BerkeleyDatabase> MakeBerkeleyDatabase(const fs::path& path, const DatabaseOptions& options, DatabaseStatus& status, std::string& error)
{
    status = DatabaseStatus::FAILED_LOAD;
    error.clear();

    if (options.require_create &&
        options.require_existing) {
        status = DatabaseStatus::FAILED_BAD_PATH;
        error =
            "Database options cannot require both creation "
            "and an existing database.";
        return nullptr;
    }

    const fs::path data_file = BDBDataFile(path);
    const bool exists = fs::exists(data_file);

    if (options.require_existing && !exists) {
        status = DatabaseStatus::FAILED_NOT_FOUND;
        error = strprintf(
            "Wallet database %s does not exist.",
            data_file.string());
        return nullptr;
    }

    if (options.require_create && exists) {
        status = DatabaseStatus::FAILED_ALREADY_EXISTS;
        error = strprintf(
            "Wallet database %s already exists.",
            data_file.string());
        return nullptr;
    }

    try {
        auto database =
            std::make_unique<BerkeleyDatabase>(path);

        status = DatabaseStatus::SUCCESS;
        return database;
    } catch (const std::exception& exception) {
        status = exists
            ? DatabaseStatus::FAILED_LOAD
            : DatabaseStatus::FAILED_CREATE;

        error = exception.what();
        return nullptr;
    }
}

std::unique_ptr<WalletDatabase> MakeMockBerkeleyDatabase()
{
    return std::make_unique<BerkeleyDatabase>(
        "",
        true /* mock */);
}