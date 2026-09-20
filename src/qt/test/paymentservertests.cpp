// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2018-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license.

#include "paymentservertests.h"

#include <QTest>

void PaymentServerTests::paymentServerTests()
{
    QSKIP("BIP70 payment request tests temporarily disabled during BIP21 modernization.");
}

void RecipientCatcher::getRecipient(SendCoinsRecipient r)
{
    recipient = r;
}