// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2018-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license.

#include "paymentservertests.h"

#include "base58.h"
#include "chainparams.h"
#include "key.h"

#include <QTest>

void PaymentServerTests::paymentServerTests()
{
    SelectParams(CBaseChainParams::MAIN);

    PaymentServer server(nullptr, false);
    RecipientCatcher catcher;

    QVERIFY(QObject::connect(
        &server,
        SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
        &catcher,
        SLOT(getRecipient(SendCoinsRecipient))));

    server.uiReady();

    CKey key;
    key.MakeNewKey(true);

    const QString address = QString::fromStdString(
        CBitcoinAddress(key.GetPubKey().GetID()).ToString());

    const QString uri = QString(
        "schillingcoin:%1?amount=1.25&label=Test&message=BIP21")
        .arg(address);

    server.handleURIOrFile(uri);

    QCOMPARE(catcher.recipient.address, address);
    QCOMPARE(catcher.recipient.amount, static_cast<CAmount>(125000000));
    QCOMPARE(catcher.recipient.label, QString("Test"));
    QCOMPARE(catcher.recipient.message, QString("BIP21"));
}

void RecipientCatcher::getRecipient(SendCoinsRecipient r)
{
    recipient = r;
}