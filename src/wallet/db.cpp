// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/db.h"

#include "wallet/bdb.h"

std::unique_ptr<WalletDatabase> MakeDatabase(
    const fs::path& path,
    const DatabaseOptions& options,
    DatabaseStatus& status,
    std::string& error)
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

    return MakeBerkeleyDatabase(
        path,
        options,
        status,
        error);
}

std::unique_ptr<WalletDatabase> CreateDummyWalletDatabase()
{
    return std::make_unique<DummyDatabase>();
}

std::unique_ptr<WalletDatabase> CreateMockWalletDatabase()
{
    return MakeMockBerkeleyDatabase();
}