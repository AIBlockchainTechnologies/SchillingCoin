// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "paymentserver.h"

#include "base58.h"
#include "chainparams.h"
#include "guiinterface.h"
#include "guiutil.h"
#include "util.h"

#include <QDataStream>
#include <QEvent>
#include <QFileOpenEvent>
#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>

#include <vector>

const int BITCOIN_IPC_CONNECT_TIMEOUT = 1000;
const QString BITCOIN_IPC_PREFIX("schillingcoin:");

namespace
{
std::vector<QString> savedPaymentRequests;

QString ipcServerName()
{
    QString name("SchillingCoinQt");
    const QString dataDirectory = QString::fromStdString(GetDataDir(true).string());
    name.append(QString::number(qHash(dataDirectory)));
    return name;
}
}

void PaymentServer::ipcParseCommandLine(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        const QString argument(argv[i]);

        if (argument.startsWith("-")) {
            continue;
        }

        if (!argument.startsWith(BITCOIN_IPC_PREFIX, Qt::CaseInsensitive)) {
            continue;
        }

        savedPaymentRequests.push_back(argument);

        SendCoinsRecipient recipient;
        if (GUIUtil::parseBitcoinURI(argument, &recipient) && !recipient.address.isEmpty()) {
            const CBitcoinAddress address(recipient.address.toStdString());

            if (address.IsValid(Params(CBaseChainParams::MAIN))) {
                SelectParams(CBaseChainParams::MAIN);
            } else if (address.IsValid(Params(CBaseChainParams::TESTNET))) {
                SelectParams(CBaseChainParams::TESTNET);
            }
        }
    }
}


void PaymentServer::LoadRootCAs()
{
    // BIP-70 removed; compatibility stub.
}

bool PaymentServer::ipcSendCommandLine()
{
    bool sentAnyRequest = false;

    for (const QString& request : savedPaymentRequests) {
        QLocalSocket socket;
        socket.connectToServer(ipcServerName(), QIODevice::WriteOnly);

        if (!socket.waitForConnected(BITCOIN_IPC_CONNECT_TIMEOUT)) {
            return false;
        }

        QByteArray block;
        QDataStream stream(&block, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_4_0);
        stream << request;

        socket.write(block);
        socket.flush();
        socket.waitForBytesWritten(BITCOIN_IPC_CONNECT_TIMEOUT);
        socket.disconnectFromServer();
        sentAnyRequest = true;
    }

    return sentAnyRequest;
}

PaymentServer::PaymentServer(QObject* parent, bool startLocalServer) :
    QObject(parent),
    saveURIs(true),
    uriServer(nullptr),
    netManager(nullptr),
    optionsModel(nullptr)
{
    if (parent) {
        parent->installEventFilter(this);
    }

    const QString name = ipcServerName();
    QLocalServer::removeServer(name);

    if (!startLocalServer) {
        return;
    }

    uriServer = new QLocalServer(this);

    if (!uriServer->listen(name)) {
        QMessageBox::critical(nullptr,
                              tr("Payment request error"),
                              tr("Cannot start schillingcoin: click-to-pay handler"));
        return;
    }

    connect(uriServer, SIGNAL(newConnection()), this, SLOT(handleURIConnection()));
}

PaymentServer::~PaymentServer() = default;

bool PaymentServer::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent* fileEvent = static_cast<QFileOpenEvent*>(event);

        if (!fileEvent->url().isEmpty()) {
            handleURIOrFile(fileEvent->url().toString());
            return true;
        }

        if (!fileEvent->file().isEmpty()) {
            handleURIOrFile(fileEvent->file());
            return true;
        }
    }

    return QObject::eventFilter(object, event);
}

void PaymentServer::initNetManager()
{
    // BIP-70 network fetching is intentionally disabled in this temporary stub.
}

void PaymentServer::uiReady()
{
    saveURIs = false;

    for (const QString& request : savedPaymentRequests) {
        handleURIOrFile(request);
    }

    savedPaymentRequests.clear();
}

void PaymentServer::handleURIOrFile(const QString& value)
{
    if (saveURIs) {
        savedPaymentRequests.push_back(value);
        return;
    }

    if (!value.startsWith(BITCOIN_IPC_PREFIX, Qt::CaseInsensitive)) {
        Q_EMIT message(tr("URI handling"),
                       tr("Only schillingcoin: payment URIs are supported."),
                       CClientUIInterface::ICON_WARNING);
        return;
    }

    SendCoinsRecipient recipient;
    if (!GUIUtil::parseBitcoinURI(value, &recipient)) {
        Q_EMIT message(tr("URI handling"),
                       tr("URI cannot be parsed! This can be caused by an invalid SchillingCoin address or malformed URI parameters."),
                       CClientUIInterface::ICON_WARNING);
        return;
    }

    const CBitcoinAddress address(recipient.address.toStdString());
    if (!address.IsValid()) {
        Q_EMIT message(tr("URI handling"),
                       tr("Invalid payment address %1").arg(recipient.address),
                       CClientUIInterface::MSG_ERROR);
        return;
    }

    Q_EMIT receivedPaymentRequest(recipient);
}

void PaymentServer::handleURIConnection()
{
    if (!uriServer) {
        return;
    }

    QLocalSocket* clientConnection = uriServer->nextPendingConnection();
    if (!clientConnection) {
        return;
    }

    while (clientConnection->bytesAvailable() < static_cast<qint64>(sizeof(quint32))) {
        if (!clientConnection->waitForReadyRead()) {
            return;
        }
    }

    connect(clientConnection, SIGNAL(disconnected()),
            clientConnection, SLOT(deleteLater()));

    QDataStream stream(clientConnection);
    stream.setVersion(QDataStream::Qt_4_0);

    QString messageValue;
    stream >> messageValue;
    handleURIOrFile(messageValue);
}

void PaymentServer::setOptionsModel(OptionsModel* optionsModel)
{
    this->optionsModel = optionsModel;
}