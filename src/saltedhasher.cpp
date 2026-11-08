// Copyright (c) 2019-2025 The Dash Core developers
// Copyright (c) 2026 The SchilingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <saltedhasher.h>
#include <random.h>

SaltedHasherBase::SaltedHasherBase() : k0(FastRandomContext().rand64()), k1(FastRandomContext().rand64()) {}

SaltedHasherBase StaticSaltedHasher::s;