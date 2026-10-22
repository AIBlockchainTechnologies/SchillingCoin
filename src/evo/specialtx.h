// Copyright (c) 2018-2024 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_EVO_SPECIALTX_H
#define SCHILLINGCOIN_EVO_SPECIALTX_H

#include "primitives/transaction.h"
#include "streams.h"
#include "version.h"

#include <exception>
#include <vector>

template <typename T>
inline bool GetTxPayload(const std::vector<unsigned char>& payload, T& obj)
{
    CDataStream ds(payload, SER_NETWORK, PROTOCOL_VERSION);

    try {
        ds >> obj;
    } catch (const std::exception&) {
        return false;
    }

    return ds.empty();
}

template <typename T>
inline bool GetTxPayload(const CMutableTransaction& tx, T& obj, bool assertType = true)
{
    if (assertType && tx.nType != T::SPECIALTX_TYPE)
        return false;

    return tx.nType == T::SPECIALTX_TYPE &&
           GetTxPayload(tx.vExtraPayload, obj);
}

template <typename T>
inline bool GetTxPayload(const CTransaction& tx, T& obj, bool assertType = true)
{
    if (assertType && tx.nType != T::SPECIALTX_TYPE)
        return false;

    return tx.nType == T::SPECIALTX_TYPE &&
           GetTxPayload(tx.vExtraPayload, obj);
}

template <typename T>
inline void SetTxPayload(CMutableTransaction& tx, const T& payload)
{
    CDataStream ds(SER_NETWORK, PROTOCOL_VERSION);
    ds << payload;

    tx.vExtraPayload.assign(ds.begin(), ds.end());
}

uint256 CalcTxInputsHash(const CTransaction& tx);
uint256 CalcTxOutputsHash(const CTransaction& tx);

#endif // SCHILLINGCOIN_EVO_SPECIALTX_H