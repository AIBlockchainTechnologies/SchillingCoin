// Copyright (c) 2015-2017 The Bitcoin Core developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Functionality for communicating with Tor.
 */
#ifndef SCHILLINGCOIN_TORCONTROL_H
#define SCHILLINGCOIN_TORCONTROL_H

#include <string>

#include <boost/function.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>

constexpr int DEFAULT_TOR_CONTROL_PORT = 9051;
extern const std::string DEFAULT_TOR_CONTROL;
static const bool DEFAULT_LISTEN_ONION = true;

// Correct signature — matches torcontrol.cpp
void StartTorControl();
void InterruptTorControl();
void StopTorControl();

#endif // SCHILLINGCOIN_TORCONTROL_H