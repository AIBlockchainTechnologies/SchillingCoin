// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Zerocoin Test Suite
#define BOOST_TEST_MAIN

#include "stubs/zerocoin_legacy_consensus.h"S
#include "amount.h"
#include "chainparams.h"
#include "main.h"
#include "txdb.h"

#include <boost/test/unit_test.hpp>
#include <iostream>

struct ZeroSetup {
    ZeroSetup() {
        std::cout << "global setup\n";
    }
    ~ZeroSetup()
    {
        std::cout << "global teardown\n";
    }
};

BOOST_GLOBAL_FIXTURE(ZeroSetup);