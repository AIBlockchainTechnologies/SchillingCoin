// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactiontablemodel.h"

#include "addresstablemodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "transactiondesc.h"
#include "transactionrecord.h"
#include "walletmodel.h"

#include "main.h"
#include "sync.h"
#include "uint256.h"
#include "util.h"
#include "wallet/wallet.h"

#include <algorithm>

#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QIcon>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QThreadPool>

#include <vector>

#define SINGLE_THREAD_MAX_TXES_SIZE 4000
#define MAX_AMOUNT_LOADED_RECORDS 20000

static int column_alignments[] = {
    Qt::AlignLeft | Qt::AlignVCenter,
    Qt::AlignLeft | Qt::AlignVCenter,
    Qt::AlignLeft | Qt::AlignVCenter,
    Qt::AlignLeft | Qt::AlignVCenter,
    Qt::AlignLeft | Qt::AlignVCenter,
    Qt::AlignRight | Qt::AlignVCenter
};

struct TxLessThan {
    bool operator()(const TransactionRecord& a, const TransactionRecord& b) const { return a.hash < b.hash; }
    bool operator()(const TransactionRecord& a, const uint256& b) const { return a.hash < b; }
    bool operator()(const uint256& a, const TransactionRecord& b) const { return a < b.hash; }
};

struct ConvertTxToVectorResult
{
    std::vector<TransactionRecord> records;
    qint64 nFirstLoadedTxTime{0};
};

class TransactionTablePriv
{
public:
    TransactionTablePriv(CWallet* wallet, TransactionTableModel* parent) :
        wallet(wallet), parent(parent)
    {
    }

    CWallet* wallet;
    TransactionTableModel* parent;

    std::vector<TransactionRecord> cachedWallet;
    qint64 nFirstLoadedTxTime{0};

    void refreshWallet()
    {
        qDebug() << "TransactionTablePriv::refreshWallet";
        cachedWallet.clear();

        std::vector<CWalletTx> walletTxes = wallet->getWalletTxs();
        std::size_t txesSize = walletTxes.size();

        if (txesSize > SINGLE_THREAD_MAX_TXES_SIZE) {

            if (txesSize > MAX_AMOUNT_LOADED_RECORDS) {
                sort(walletTxes.begin(), walletTxes.end(),
                     [](const CWalletTx& a, const CWalletTx& b) { return a.GetTxTime() > b.GetTxTime(); });

                walletTxes = std::vector<CWalletTx>(walletTxes.begin(),
                                                    walletTxes.begin() + MAX_AMOUNT_LOADED_RECORDS);
                txesSize = walletTxes.size();
            }

            std::size_t threadsCount = (QThreadPool::globalInstance()->maxThreadCount() / 2) + 1;
            std::size_t subsetSize = txesSize / (threadsCount + 1);
            std::size_t totalSumSize = 0;

            std::vector<QFuture<ConvertTxToVectorResult>> tasks;

            for (std::size_t i = 0; i < threadsCount; ++i) {
                tasks.push_back(
                    QtConcurrent::run(
                        convertTxToRecords,
                        this,
                        wallet,
                        std::vector<CWalletTx>(walletTxes.begin() + totalSumSize,
                                               walletTxes.begin() + totalSumSize + subsetSize)
                    )
                );
                totalSumSize += subsetSize;
            }

            std::size_t remainingSize = txesSize - totalSumSize;
            auto res = convertTxToRecords(this, wallet,
                                          std::vector<CWalletTx>(walletTxes.end() - remainingSize,
                                                                 walletTxes.end()));

            cachedWallet.insert(cachedWallet.end(), res.records.begin(), res.records.end());
            nFirstLoadedTxTime = res.nFirstLoadedTxTime;

            for (auto& future : tasks) {
                future.waitForFinished();
                ConvertTxToVectorResult convertRes = future.result();

                cachedWallet.insert(cachedWallet.end(),
                                    convertRes.records.begin(),
                                    convertRes.records.end());

                if (nFirstLoadedTxTime > convertRes.nFirstLoadedTxTime)
                    nFirstLoadedTxTime = convertRes.nFirstLoadedTxTime;
            }

        } else {
            ConvertTxToVectorResult convertRes = convertTxToRecords(this, wallet, walletTxes);
            cachedWallet.insert(cachedWallet.end(),
                                convertRes.records.begin(),
                                convertRes.records.end());
            nFirstLoadedTxTime = convertRes.nFirstLoadedTxTime;
        }
    }

    static ConvertTxToVectorResult convertTxToRecords(TransactionTablePriv* tablePriv,
                                                      const CWallet* wallet,
                                                      const std::vector<CWalletTx>& walletTxes)
    {
        Q_UNUSED(tablePriv);
        ConvertTxToVectorResult res;

        for (const auto& tx : walletTxes) {
            std::vector<TransactionRecord> records =
                TransactionRecord::decomposeTransaction(wallet, tx);

            if (!records.empty()) {
                qint64 time = records.front().time;
                if (res.nFirstLoadedTxTime == 0 || res.nFirstLoadedTxTime > time)
                    res.nFirstLoadedTxTime = time;
            }

            res.records.insert(res.records.end(), records.begin(), records.end());
        }

        return res;
    }

    void updateWallet(const uint256& hash, int status, bool showTransaction, TransactionRecord& ret)
    {
        qDebug() << "TransactionTablePriv::updateWallet : "
                 << QString::fromStdString(hash.ToString()) << " " << status;

        auto lower = std::lower_bound(cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
        auto upper = std::upper_bound(cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());

        int lowerIndex = lower - cachedWallet.begin();
        int upperIndex = upper - cachedWallet.begin();
        bool inModel = (lower != upper);

        if (status == CT_UPDATED) {
            if (showTransaction && !inModel)
                status = CT_NEW;
            if (!showTransaction && inModel)
                status = CT_DELETED;
        }

        qDebug() << "    inModel=" << inModel
                 << " Index=" << lowerIndex << "-" << upperIndex
                 << " showTransaction=" << showTransaction
                 << " derivedStatus=" << status;

        switch (status) {
        case CT_NEW:
            if (inModel) {
                qWarning() << "TransactionTablePriv::updateWallet : Warning: Got CT_NEW but already in model";
                break;
            }
            if (showTransaction) {
                LOCK2(cs_main, wallet->cs_wallet);

                auto mi = wallet->mapWallet.find(hash);
                if (mi == wallet->mapWallet.end()) {
                    qWarning() << "TransactionTablePriv::updateWallet : Warning: Got CT_NEW but not in wallet";
                    break;
                }

                const CWalletTx& wtx = mi->second;

                if (cachedWallet.size() >= MAX_AMOUNT_LOADED_RECORDS &&
                    wtx.GetTxTime() < nFirstLoadedTxTime)
                    return;

                std::vector<TransactionRecord> toInsert =
                    TransactionRecord::decomposeTransaction(wallet, wtx);

                if (!toInsert.empty()) {
                    parent->beginInsertRows(QModelIndex(),
                                            lowerIndex,
                                            lowerIndex + toInsert.size() - 1);

                    int insert_idx = lowerIndex;
                    for (const TransactionRecord& rec : toInsert) {
                        cachedWallet.insert(cachedWallet.begin() + insert_idx, rec);
                        insert_idx++;
                        ret = rec;
                    }

                    parent->endInsertRows();
                }
            }
            break;

        case CT_DELETED:
            if (!inModel) {
                qWarning() << "TransactionTablePriv::updateWallet : Warning: Got CT_DELETED but not in model";
                break;
            }

            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex - 1);
            cachedWallet.erase(lower, upper);
            parent->endRemoveRows();
            break;

        case CT_UPDATED:
            break;
        }
    }

    int size() { return cachedWallet.size(); }

    TransactionRecord* index(int idx)
    {
        if (idx >= 0 && idx < (int)cachedWallet.size()) {
            TransactionRecord* rec = &cachedWallet[idx];

            TRY_LOCK(cs_main, lockMain);
            if (lockMain) {
                TRY_LOCK(wallet->cs_wallet, lockWallet);
                if (lockWallet && rec->statusUpdateNeeded()) {
                    auto mi = wallet->mapWallet.find(rec->hash);
                    if (mi != wallet->mapWallet.end())
                        rec->updateStatus(mi->second);
                }
            }
            return rec;
        }
        return nullptr;
    }

    QString describe(TransactionRecord* rec, int unit)
    {
        LOCK2(cs_main, wallet->cs_wallet);
        auto mi = wallet->mapWallet.find(rec->hash);
        if (mi != wallet->mapWallet.end())
            return TransactionDesc::toHTML(wallet, mi->second, rec, unit);

        return QString();
    }
};

TransactionTableModel::TransactionTableModel(CWallet* wallet, WalletModel* parent) : QAbstractTableModel(parent),
                                                                                     wallet(wallet),
                                                                                     walletModel(parent),
                                                                                     priv(new TransactionTablePriv(wallet, this)),
                                                                                     fProcessingQueuedTransactions(false)
{
    columns << QString() << QString() << tr("Date") << tr("Type") << tr("Address") << BitcoinUnits::getAmountColumnTitle(walletModel->getOptionsModel()->getDisplayUnit());
    priv->refreshWallet();

    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

    subscribeToCoreSignals();
}

TransactionTableModel::~TransactionTableModel()
{
    unsubscribeFromCoreSignals();
    delete priv;
}

/** Updates the column title to "Amount (DisplayUnit)" and emits headerDataChanged() signal for table headers to react. */
void TransactionTableModel::updateAmountColumnTitle()
{
    columns[Amount] = BitcoinUnits::getAmountColumnTitle(walletModel->getOptionsModel()->getDisplayUnit());
    Q_EMIT headerDataChanged(Qt::Horizontal, Amount, Amount);
}

void TransactionTableModel::updateTransaction(const QString& hash, int status, bool showTransaction)
{
    uint256 updated;
    updated.SetHex(hash.toStdString());

    TransactionRecord rec(0);
    priv->updateWallet(updated, status, showTransaction, rec);

    if (!rec.isNull())
        Q_EMIT txArrived(hash, rec.isCoinStake(), rec.isAnyColdStakingType(), rec.isMasternodeReward());
}

void TransactionTableModel::updateConfirmations()
{
    // Blocks came in since last poll.
    // Invalidate status (number of confirmations) and (possibly) description
    //  for all rows. Qt is smart enough to only actually request the data for the
    //  visible rows.
    if (priv->size() > 0) {
        Q_EMIT dataChanged(index(0, Status), index(priv->size() - 1, Status));
        Q_EMIT dataChanged(index(0, ToAddress), index(priv->size() - 1, ToAddress));
    }
}

int TransactionTableModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int TransactionTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

int TransactionTableModel::size() const
{
    return priv->size();
}

QString TransactionTableModel::formatTxStatus(const TransactionRecord* wtx) const
{
    QString status;

    switch (wtx->status.status) {
    case TransactionStatus::OpenUntilBlock:
        status = tr("Open for %n more block(s)", "", wtx->status.open_for);
        break;
    case TransactionStatus::OpenUntilDate:
        status = tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx->status.open_for));
        break;
    case TransactionStatus::Offline:
        status = tr("Offline");
        break;
    case TransactionStatus::Unconfirmed:
        status = tr("Unconfirmed");
        break;
    case TransactionStatus::Confirming:
        status = tr("Confirming (%1 of %2 recommended confirmations)").arg(wtx->status.depth).arg(TransactionRecord::RecommendedNumConfirmations);
        break;
    case TransactionStatus::Confirmed:
        status = tr("Confirmed (%1 confirmations)").arg(wtx->status.depth);
        break;
    case TransactionStatus::Conflicted:
        status = tr("Conflicted");
        break;
    case TransactionStatus::Immature:
        status = tr("Immature (%1 confirmations, will be available after %2)").arg(wtx->status.depth).arg(wtx->status.depth + wtx->status.matures_in);
        break;
    case TransactionStatus::MaturesWarning:
        status = tr("This block was not received by any other nodes and will probably not be accepted!");
        break;
    case TransactionStatus::NotAccepted:
        status = tr("Orphan Block - Generated but not accepted. This does not impact your holdings.");
        break;
    }

    return status;
}

QString TransactionTableModel::formatTxDate(const TransactionRecord* wtx) const
{
    if (wtx->time) {
        return GUIUtil::dateTimeStr(wtx->time);
    }
    return QString();
}

/* Look up address in address book, if found return label (address)
   otherwise just return (address)
 */
QString TransactionTableModel::lookupAddress(const std::string& address, bool tooltip) const
{
    QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(address));
    QString description;
    if (!label.isEmpty()) {
        description += label + QString(" ");
    }
    if (label.isEmpty() || tooltip) {
        description += QString::fromStdString(address);
    }
    return description;
}

QString TransactionTableModel::formatTxType(const TransactionRecord* wtx) const
{
    switch (wtx->type) {
    case TransactionRecord::RecvWithAddress:
        return tr("Received with");
    case TransactionRecord::MNReward:
        return tr("Masternode Reward");
    case TransactionRecord::RecvFromOther:
        return tr("Received from");
    case TransactionRecord::RecvWithObfuscation:
        return tr("Received via Obfuscation");
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return tr("Sent to");
    case TransactionRecord::SendToSelf:
        return tr("Payment to yourself");
    case TransactionRecord::StakeMint:
        return tr("SCH Stake");
    case TransactionRecord::StakeDelegated:
        return tr("SCH Cold Stake");
    case TransactionRecord::StakeHot:
        return tr("SCH Stake on behalf of");
    case TransactionRecord::P2CSDelegationSent:
    case TransactionRecord::P2CSDelegationSentOwner:
    case TransactionRecord::P2CSDelegation:
        return tr("Stake delegation");
    case TransactionRecord::P2CSUnlockOwner:
    case TransactionRecord::P2CSUnlockStaker:
        return tr("Stake delegation spent by");
    case TransactionRecord::Generated:
        return tr("Mined");
    case TransactionRecord::ObfuscationDenominate:
        return tr("Obfuscation Denominate");
    case TransactionRecord::ObfuscationCollateralPayment:
        return tr("Obfuscation Collateral Payment");
    case TransactionRecord::ObfuscationMakeCollaterals:
        return tr("Obfuscation Make Collateral Inputs");
    case TransactionRecord::ObfuscationCreateDenominations:
        return tr("Obfuscation Create Denominations");
    case TransactionRecord::Obfuscated:
        return tr("Obfuscated");
    default:
        return QString();
    }
}

QVariant TransactionTableModel::txAddressDecoration(const TransactionRecord* wtx) const
{
    switch (wtx->type) {
    case TransactionRecord::Generated:
    case TransactionRecord::StakeMint:
    case TransactionRecord::MNReward:
        return QIcon(":/icons/tx_mined");
    case TransactionRecord::RecvWithObfuscation:
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::RecvFromOther:
        return QIcon(":/icons/tx_input");
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return QIcon("://ic-transaction-sent");
    default:
        return QIcon(":/icons/tx_inout");
    }
}

QString TransactionTableModel::formatTxToAddress(const TransactionRecord* wtx, bool tooltip) const
{
    QString watchAddress;
    if (tooltip) {
        // Mark transactions involving watch-only addresses by adding " (watch-only)"
        watchAddress = wtx->involvesWatchAddress ? QString(" (") + tr("watch-only") + QString(")") : "";
    }

    switch (wtx->type) {
    case TransactionRecord::RecvFromOther:
        return QString::fromStdString(wtx->address) + watchAddress;
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::MNReward:
    case TransactionRecord::RecvWithObfuscation:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::Generated:
    case TransactionRecord::StakeMint:
        return lookupAddress(wtx->address, tooltip);
    case TransactionRecord::Obfuscated:
        return lookupAddress(wtx->address, tooltip) + watchAddress;
    case TransactionRecord::SendToOther:
        return QString::fromStdString(wtx->address) + watchAddress;
    case TransactionRecord::P2CSDelegation:
    case TransactionRecord::P2CSDelegationSent:
    case TransactionRecord::P2CSDelegationSentOwner:
    case TransactionRecord::P2CSUnlockOwner:
    case TransactionRecord::P2CSUnlockStaker:
    case TransactionRecord::StakeDelegated:
    case TransactionRecord::StakeHot:
    case TransactionRecord::SendToSelf: {
        QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(wtx->address));
        return label.isEmpty() ? "" : label;
    }
    default: {
        if (watchAddress.isEmpty()) {
            return tr("No information");
        } else {
            return tr("(n/a)") + watchAddress;
        }
    }
    }
}

QVariant TransactionTableModel::addressColor(const TransactionRecord* wtx) const
{
    switch (wtx->type) {
    // Show addresses without label in a less visible color
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::Generated:
    case TransactionRecord::MNReward: {
        QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(wtx->address));
        if (label.isEmpty())
            return COLOR_BAREADDRESS;
    }
    case TransactionRecord::SendToSelf:
    default:
        // To avoid overriding above conditional formats a default text color for this QTableView is not defined in stylesheet,
        // so we must always return a color here
        return COLOR_BLACK;
    }
}

QString TransactionTableModel::formatTxAmount(const TransactionRecord* wtx, bool showUnconfirmed, BitcoinUnits::SeparatorStyle separators) const
{
    QString str = BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), wtx->credit + wtx->debit, false, separators);
    if (showUnconfirmed) {
        if (!wtx->status.countsForBalance) {
            str = QString("[") + str + QString("]");
        }
    }
    return QString(str);
}

QVariant TransactionTableModel::txStatusDecoration(const TransactionRecord* wtx) const
{
    switch (wtx->status.status) {
    case TransactionStatus::OpenUntilBlock:
    case TransactionStatus::OpenUntilDate:
        return COLOR_TX_STATUS_OPENUNTILDATE;
    case TransactionStatus::Offline:
        return COLOR_TX_STATUS_OFFLINE;
    case TransactionStatus::Unconfirmed:
        return QIcon(":/icons/transaction_0");
    case TransactionStatus::Confirming:
        switch (wtx->status.depth) {
        case 1:
            return QIcon(":/icons/transaction_1");
        case 2:
            return QIcon(":/icons/transaction_2");
        case 3:
            return QIcon(":/icons/transaction_3");
        case 4:
            return QIcon(":/icons/transaction_4");
        default:
            return QIcon(":/icons/transaction_5");
        };
    case TransactionStatus::Confirmed:
        return QIcon(":/icons/transaction_confirmed");
    case TransactionStatus::Conflicted:
        return QIcon(":/icons/transaction_conflicted");
    case TransactionStatus::Immature: {
        int total = wtx->status.depth + wtx->status.matures_in;
        int part = (wtx->status.depth * 5 / total) + 1;
        return QIcon(QString(":/icons/transaction_%1").arg(part));
    }
    case TransactionStatus::MaturesWarning:
    case TransactionStatus::NotAccepted:
        return QIcon(":/icons/transaction_0");
    default:
        return COLOR_BLACK;
    }
}

QVariant TransactionTableModel::txWatchonlyDecoration(const TransactionRecord* wtx) const
{
    if (wtx->involvesWatchAddress)
        return QIcon(":/icons/eye");
    else
        return QVariant();
}

QString TransactionTableModel::formatTooltip(const TransactionRecord* rec) const
{
    return formatTxStatus(rec);
}

QVariant TransactionTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();
    TransactionRecord* rec = static_cast<TransactionRecord*>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (index.column()) {
        case Status:
            return txStatusDecoration(rec);
        case Watchonly:
            return txWatchonlyDecoration(rec);
        case ToAddress:
            return txAddressDecoration(rec);
        }
        break;
    case Qt::DisplayRole:
        switch (index.column()) {
        case Date:
            return formatTxDate(rec);
        case Type:
            return formatTxType(rec);
        case ToAddress:
            return formatTxToAddress(rec, false);
        case Amount:
            return formatTxAmount(rec, true, BitcoinUnits::separatorAlways);
        }
        break;
    case Qt::EditRole:
        // Edit role is used for sorting, so return the unformatted values
        switch (index.column()) {
        case Status:
            return QString::fromStdString(rec->status.sortKey);
        case Date:
            return rec->time;
        case Type:
            return formatTxType(rec);
        case Watchonly:
            return (rec->involvesWatchAddress ? 1 : 0);
        case ToAddress:
            return formatTxToAddress(rec, true);
        case Amount:
            return qint64(rec->credit + rec->debit);
        }
        break;
    case Qt::ToolTipRole:
        return formatTooltip(rec);
    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];
    case Qt::ForegroundRole:
        // Minted
        if (rec->type == TransactionRecord::Generated || rec->type == TransactionRecord::StakeMint || rec->type == TransactionRecord::MNReward) {
            if (rec->status.status == TransactionStatus::Conflicted || rec->status.status == TransactionStatus::NotAccepted)
                return COLOR_ORPHAN;
            else
                return COLOR_STAKE;
        }
        // Conflicted tx
        if (rec->status.status == TransactionStatus::Conflicted || rec->status.status == TransactionStatus::NotAccepted) {
            return COLOR_CONFLICTED;
        }
        // Unconfimed or immature
        if ((rec->status.status == TransactionStatus::Unconfirmed) || (rec->status.status == TransactionStatus::Immature)) {
            return COLOR_UNCONFIRMED;
        }
        if (index.column() == Amount && (rec->credit + rec->debit) < 0) {
            return COLOR_NEGATIVE;
        }
        if (index.column() == ToAddress) {
            return addressColor(rec);
        }

        // To avoid overriding above conditional formats a default text color for this QTableView is not defined in stylesheet,
        // so we must always return a color here
        return COLOR_BLACK;
    case TypeRole:
        return rec->type;
    case SizeRole:
        return rec->size;
    case DateRole:
        return QDateTime::fromTime_t(static_cast<uint>(rec->time));
    case WatchonlyRole:
        return rec->involvesWatchAddress;
    case WatchonlyDecorationRole:
        return txWatchonlyDecoration(rec);
    case LongDescriptionRole:
        return priv->describe(rec, walletModel->getOptionsModel()->getDisplayUnit());
    case AddressRole:
        return QString::fromStdString(rec->address);
    case LabelRole:
        return walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(rec->address));
    case AmountRole:
        return qint64(rec->credit + rec->debit);
    case TxIDRole:
        return rec->getTxID();
    case TxHashRole:
        return QString::fromStdString(rec->hash.ToString());
    case ConfirmedRole:
        return rec->status.countsForBalance;
    case FormattedAmountRole:
        // Used for copy/export, so don't include separators
        return formatTxAmount(rec, false, BitcoinUnits::separatorNever);
    case StatusRole:
        return rec->status.status;
    }
    return QVariant();
}

QVariant TransactionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            return columns[section];
        } else if (role == Qt::TextAlignmentRole) {
            return column_alignments[section];
        } else if (role == Qt::ToolTipRole) {
            switch (section) {
            case Status:
                return tr("Transaction status. Hover over this field to show number of confirmations.");
            case Date:
                return tr("Date and time that the transaction was received.");
            case Type:
                return tr("Type of transaction.");
            case Watchonly:
                return tr("Whether or not a watch-only address is involved in this transaction.");
            case ToAddress:
                return tr("Destination address of transaction.");
            case Amount:
                return tr("Amount removed from or added to balance.");
            }
        }
    }
    return QVariant();
}

QModelIndex TransactionTableModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    TransactionRecord* data = priv->index(row);
    if (data) {
        return createIndex(row, column, data);
    }
    return QModelIndex();
}

void TransactionTableModel::updateDisplayUnit()
{
    updateAmountColumnTitle();
    if (priv->size() > 0) {
        Q_EMIT dataChanged(index(0, Amount), index(priv->size() - 1, Amount));
    }
}

Qt::ItemFlags TransactionTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return flags;
}

QString TransactionTableModel::getTxHash(int row) const
{
    TransactionRecord* rec = priv->index(row);
    if (rec)
        return QString::fromStdString(rec->hash.ToString());
    return QString();
}

TransactionRecord* TransactionTableModel::getTxRecord(int row) const
{
    return priv->index(row);
}

void TransactionTableModel::subscribeToCoreSignals()
{
    m_connNotifyTransactionChanged =
        wallet->NotifyTransactionChanged.connect(
            [this](CWallet* wallet, const uint256& hash, ChangeType status) {

                QMetaObject::invokeMethod(
                    this,
                    [this, wallet, hash, status]() {

                        QString qHash = QString::fromStdString(hash.ToString());

                        bool showTransaction = true;
                        {
                            LOCK(wallet->cs_wallet);
                            auto it = wallet->mapWallet.find(hash);
                            showTransaction = (it != wallet->mapWallet.end());
                        }

                        updateTransaction(qHash, status, showTransaction);
                    },
                    Qt::QueuedConnection
                );
            }
        );
}

void TransactionTableModel::unsubscribeFromCoreSignals()
{
    if (m_connNotifyTransactionChanged.connected())
        m_connNotifyTransactionChanged.disconnect();
}