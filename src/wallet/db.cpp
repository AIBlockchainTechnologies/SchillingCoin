// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/db.h"

#include "chainparams.h"
#include "common/args.h"
#include "logging.h"
#include "util/fs.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace wallet {

std::vector<std::pair<fs::path, std::string>> ListDatabases(
    const fs::path& wallet_dir)
{
    std::vector<std::pair<fs::path, std::string>> paths;
    std::error_code ec;

    for (auto it = fs::recursive_directory_iterator(
             wallet_dir,
             ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec) {
            break;
        }

        try {
            const fs::path path{
                it->path().lexically_relative(wallet_dir)};

            if (it->status().type() ==
                fs::file_type::directory) {
                if (IsBDBFile(BDBDataFile(it->path()))) {
                    paths.emplace_back(path, "bdb");
                } else if (
                    IsSQLiteFile(SQLiteDataFile(it->path()))) {
                    paths.emplace_back(path, "sqlite");
                }
            } else if (
                it.depth() == 0 &&
                it->symlink_status().type() ==
                    fs::file_type::regular &&
                it->path().extension() != ".bak") {
                if (it->path().filename() == "wallet.dat") {
                    if (IsBDBFile(it->path())) {
                        paths.emplace_back(
                            fs::path(),
                            "bdb");
                    } else if (IsSQLiteFile(it->path())) {
                        paths.emplace_back(
                            fs::path(),
                            "sqlite");
                    }
                } else if (IsBDBFile(it->path())) {
                    // Preserve support for legacy top-level Berkeley
                    // wallet files whose name is not wallet.dat.
                    paths.emplace_back(path, "bdb");
                }
            }
        } catch (const std::exception& exception) {
            LogPrintf(
                "%s: Error scanning %s: %s\n",
                __func__,
                fs::PathToString(it->path()),
                exception.what());

            it.disable_recursion_pending();
        }
    }

    if (ec) {
        LogPrintf(
            "%s: Error scanning directory entries under %s: %s\n",
            __func__,
            fs::PathToString(wallet_dir),
            ec.message());
    }

    return paths;
}

fs::path BDBDataFile(const fs::path& wallet_path)
{
    if (fs::is_regular_file(wallet_path)) {
        // Backwards compatibility: an existing file path refers
        // directly to a Berkeley database file.
        return wallet_path;
    }

    return wallet_path / "wallet.dat";
}

fs::path SQLiteDataFile(const fs::path& path)
{
    return path / "wallet.dat";
}

bool IsBDBFile(const fs::path& path)
{
    if (!fs::exists(path)) {
        return false;
    }

    // A Berkeley DB Btree file has a minimum size of 4 KiB.
    // This also prevents database lock files from being opened.
    std::error_code ec;
    const auto size = fs::file_size(path, ec);

    if (ec) {
        LogPrintf(
            "%s: Error reading file size for %s: %s\n",
            __func__,
            fs::PathToString(path),
            ec.message());

        return false;
    }

    if (size < 4096) {
        return false;
    }

    std::ifstream file{path, std::ios::binary};

    if (!file.is_open()) {
        return false;
    }

    // Berkeley DB Btree magic bytes begin at byte offset 12.
    file.seekg(12, std::ios::beg);

    uint32_t data{0};
    file.read(
        reinterpret_cast<char*>(&data),
        sizeof(data));

    return file.good() &&
           (data == 0x00053162 ||
            data == 0x62310500);
}

bool IsSQLiteFile(const fs::path& path)
{
    if (!fs::exists(path)) {
        return false;
    }

    // SQLite database files have a minimum page size of 512 bytes.
    std::error_code ec;
    const auto size = fs::file_size(path, ec);

    if (ec) {
        LogPrintf(
            "%s: Error reading file size for %s: %s\n",
            __func__,
            fs::PathToString(path),
            ec.message());

        return false;
    }

    if (size < 512) {
        return false;
    }

    std::ifstream file{path, std::ios::binary};

    if (!file.is_open()) {
        return false;
    }

    char magic[16]{};
    file.read(
        magic,
        sizeof(magic));

    if (!file) {
        return false;
    }

    file.seekg(68, std::ios::beg);

    char application_id[4]{};
    file.read(
        application_id,
        sizeof(application_id));

    if (!file) {
        return false;
    }

    const std::string magic_string{
        magic,
        sizeof(magic)};

    if (magic_string !=
        std::string{"SQLite format 3\000", 16}) {
        return false;
    }

    return std::memcmp(
               Params().MessageStart().data(),
               application_id,
               sizeof(application_id)) == 0;
}

void ReadDatabaseArgs(
    const ArgsManager& args,
    DatabaseOptions& options)
{
    options.use_unsafe_sync = args.GetBoolArg(
        "-unsafesqlitesync",
        options.use_unsafe_sync);

    options.use_shared_memory = !args.GetBoolArg(
        "-privdb",
        !options.use_shared_memory);

    options.max_log_mb = args.GetIntArg(
        "-dblogsize",
        options.max_log_mb);
}

} // namespace wallet