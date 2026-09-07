// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_CLIENTMODEL_H
#define BITCOIN_QT_CLIENTMODEL_H

#include "uint256.h"
#include "chain.h"

#include <QObject>
#include <QDateTime>
#include <QTimer>

class OptionsModel;
class PeerTableModel;
class BanTableModel;

QT_BEGIN_NAMESPACE
class QDateTime;
class QTimer;
QT_END_NAMESPACE

enum BlockSource {
    BLOCK_SOURCE_NONE,
    BLOCK_SOURCE_REINDEX,
    BLOCK_SOURCE_DISK,
    BLOCK_SOURCE_NETWORK
};

enum NumConnections {
    CONNECTIONS_NONE = 0,
    CONNECTIONS_IN   = (1U << 0),
    CONNECTIONS_OUT  = (1U << 1),
    CONNECTIONS_ALL  = (CONNECTIONS_IN | CONNECTIONS_OUT),
};

/** Model for SchillingCoin network client. */
class ClientModel : public QObject
{
    Q_OBJECT

public:
    explicit ClientModel(OptionsModel* optionsModel, QObject* parent = nullptr);
    ~ClientModel() override;

    OptionsModel*    getOptionsModel();
    PeerTableModel*  getPeerTableModel();
    BanTableModel*   getBanTableModel();

    //! Return number of connections, default is in- and outbound (total)
    int      getNumConnections(unsigned int flags = CONNECTIONS_ALL) const;
    int      getNumBlocksAtStartup();
    QString  getMasternodeCountString() const;

    // from cached block index
    int       getNumBlocks();
    QDateTime getLastBlockDate() const;
    QString   getLastBlockHash() const;
    double    getVerificationProgress() const;

    quint64 getTotalBytesRecv() const;
    quint64 getTotalBytesSent() const;

    //! Return true if core is doing initial block download
    bool          inInitialBlockDownload() const;
    //! Return current block source
    enum BlockSource getBlockSource() const;
    //! Return warnings to be displayed in status bar
    QString       getStatusBarWarnings() const;

    QString formatFullVersion() const;
    QString formatFullVersionWithCodename() const;
    QString formatBuildDate() const;
    bool    isReleaseVersion() const;
    QString clientName() const;
    QString formatClientStartupTime() const;
    QString dataDir() const;

    // cache setters used by core notifications
    void setCacheTip(const CBlockIndex* const tip)      { cacheTip = tip; }
    void setCacheReindexing(bool reindex)               { cachedReindexing = reindex; }
    void setCacheImporting(bool import)                 { cachedImporting = import; }
    void setCacheInitialSync(bool _initialSync)         { cachedInitialSync = _initialSync; }

    bool getTorInfo(std::string& ip_port) const;

private:
    OptionsModel*   optionsModel;
    PeerTableModel* peerTableModel;
    BanTableModel*  banTableModel;

    const CBlockIndex* cacheTip{nullptr};
    QString            cachedMasternodeCountString;
    bool               cachedReindexing{false};
    bool               cachedImporting{false};
    bool               cachedInitialSync{false};

    int     numBlocksAtStartup{-1};
    QTimer* pollTimer{nullptr};
    QTimer* pollMnTimer{nullptr};

    /** Subscribe/unsubscribe core signals. Implemented in clientmodel.cpp using static connections. */
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

Q_SIGNALS:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count);
    void strMasternodesChanged(const QString& strMasternodes);
    void alertsChanged(const QString& warnings);
    void bytesChanged(quint64 totalBytesIn, quint64 totalBytesOut);

    //! Fired when a message should be reported to the user
    void message(const QString& title, const QString& message, unsigned int style);

    //! Initialization / progress messages marshalled from core
    void initMessage(const QString& message);
    void showProgress(const QString& title, int nProgress);

public Q_SLOTS:
    void updateTimer();
    void updateMnTimer();
    void updateNumConnections(int numConnections);
    void updateAlert();
    void updateBanlist();

    // Slots used to receive marshalled core notifications on the Qt thread
    void handleInitMessage(const QString& message);
    void handleShowProgress(const QString& title, int nProgress);

private:
    Q_DISABLE_COPY(ClientModel)
};

#endif // BITCOIN_QT_CLIENTMODEL_H