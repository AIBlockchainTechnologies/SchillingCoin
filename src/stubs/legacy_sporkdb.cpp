// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
  NOTE: Persistent CSporkDB retained.

  Reason: legacy_spork.cpp, legacy_spork.h, and sporkid.h were already
  stubbed, so replacing CSporkDB was unnecessary. The LevelDB-backed
  CSporkDB is kept to preserve durability and compatibility for production.

  Operational notes:
  - This implementation persists spork state to disk under datadir/sporks.
  - Production builds should use the persistent DB; developers may use
    the in-memory stub for tests via the DISABLE_SPORK_DB compile flag
    or the --sporkdb=memory runtime option.
  - Writes should validate SporkId and cap payload sizes to prevent abuse.
*/

#include "stubs/legacy_sporkdb.h"
#include "stubs/legacy_spork.h"

CSporkDB::CSporkDB(size_t nCacheSize, bool fMemory, bool fWipe) : CLevelDBWrapper(GetDataDir() / "sporks", nCacheSize, fMemory, fWipe) {}

bool CSporkDB::WriteSpork(const SporkId nSporkId, const CSporkMessage& spork)
{
    LogPrintf("Wrote spork %s to database\n", sporkManager.GetSporkNameByID(nSporkId));
    return Write(nSporkId, spork);

}

bool CSporkDB::ReadSpork(const SporkId nSporkId, CSporkMessage& spork)
{
    return Read(nSporkId, spork);
}

bool CSporkDB::SporkExists(const SporkId nSporkId)
{
    return Exists(nSporkId);
}