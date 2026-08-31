// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Stubbed ZeroSetup fixture
// Purpose: preserve the fixture type and lifetime semantics without performing
// console I/O or requiring Boost.Test in non-test builds.

#define BOOST_TEST_MODULE Zerocoin Test Suite
#define BOOST_TEST_MAIN

#include "stubs/legacy_zerocoin_consensus.h"
#include "amount.h"
#include "chainparams.h"
#include "main.h"
#include "txdb.h"

#include <boost/test/unit_test.hpp>
#include <iostream>

struct ZeroSetup {
    ZeroSetup()  {}
    ~ZeroSetup() {}
};

#ifdef BOOST_TEST
BOOST_GLOBAL_FIXTURE(ZeroSetup);
#else
static ZeroSetup g_zero_setup_instance;
#endif