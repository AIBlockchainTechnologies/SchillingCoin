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

#ifndef STUBS_LEGACY_SPORKDB_H
#define STUBS_LEGACY_SPORKDB_H

#include <boost/filesystem/path.hpp>
#include "leveldbwrapper.h"
#include "stubs/legacy_spork.h"

class CSporkDB : public CLevelDBWrapper
{
public:
    CSporkDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CSporkDB(const CSporkDB&);
    void operator=(const CSporkDB&);

public:
    bool WriteSpork(const SporkId nSporkId, const CSporkMessage& spork);
    bool ReadSpork(const SporkId nSporkId, CSporkMessage& spork);
    bool SporkExists(const SporkId nSporkId);
};


#endif //STUBS_LEGACY_SPORKDB_H