// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoinunits.h"
#include "chainparams.h"
#include "primitives/transaction.h"

#include <QSettings>
#include <QStringList>

#include <iostream>

BitcoinUnits::BitcoinUnits(QObject* parent) :
    QAbstractListModel(parent),
    unitlist(availableUnits())
{
}

std::vector<BitcoinUnits::Unit> BitcoinUnits::availableUnits()
{
    std::vector<BitcoinUnits::Unit> unitlist;
    unitlist.reserve(3);
    unitlist.push_back(SCH);
    unitlist.push_back(mSCH);
    unitlist.push_back(uSCH);
    return unitlist;
}

bool BitcoinUnits::valid(int unit)
{
    switch (unit) {
    case SCH:
    case mSCH:
    case uSCH:
        return true;
    default:
        return false;
    }
}

QString BitcoinUnits::id(int unit)
{
    switch (unit) {
    case SCH:
        return QString("sch");
    case mSCH:
        return QString("msch");
    case uSCH:
        return QString::fromUtf8("usch");
    default:
        return QString("???");
    }
}

QString BitcoinUnits::name(int unit)
{
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        switch (unit) {
        case SCH:
            return QString("SCH");
        case mSCH:
            return QString("mSCH");
        case uSCH:
            return QString::fromUtf8("μSCH");
        default:
            return QString("???");
        }
    } else {
        switch (unit) {
        case SCH:
            return QString("tSCH");
        case mSCH:
            return QString("mtSCH");
        case uSCH:
            return QString::fromUtf8("μtSCH");
        default:
            return QString("???");
        }
    }
}

QString BitcoinUnits::description(int unit)
{
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        switch (unit) {
        case SCH:
            return QString("SCH");
        case mSCH:
            return QString("Milli-SCH (1 / 1" THIN_SP_UTF8 "000)");
        case uSCH:
            return QString("Micro-SCH (1 / 1" THIN_SP_UTF8 "000" THIN_SP_UTF8 "000)");
        default:
            return QString("???");
        }
    } else {
        switch (unit) {
        case SCH:
            return QString("TestSCH");
        case mSCH:
            return QString("Milli-TestSCH (1 / 1" THIN_SP_UTF8 "000)");
        case uSCH:
            return QString("Micro-TestSCH (1 / 1" THIN_SP_UTF8 "000" THIN_SP_UTF8 "000)");
        default:
            return QString("???");
        }
    }
}

qint64 BitcoinUnits::factor(int unit)
{
    switch (unit) {
    case SCH:
        return 100000000;
    case mSCH:
        return 100000;
    case uSCH:
        return 100;
    default:
        return 100000000;
    }
}

int BitcoinUnits::decimals(int unit)
{
    switch (unit) {
    case SCH:
        return 8;
    case mSCH:
        return 5;
    case uSCH:
        return 2;
    default:
        return 0;
    }
}

QString BitcoinUnits::format(int unit, const CAmount& nIn, bool fPlus, SeparatorStyle separators, bool cleanRemainderZeros)
{
    if (!valid(unit))
        return QString();

    qint64 n = (qint64)nIn;
    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    qint64 remainder = n_abs % coin;
    QString quotient_str = QString::number(quotient);
    QString remainder_str = QString::number(remainder).rightJustified(num_decimals, '0');

    QChar thin_sp(THIN_SP_CP);
    int q_size = quotient_str.size();
    if (separators == separatorAlways || (separators == separatorStandard && q_size > 4))
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, thin_sp);

    if (n < 0)
        quotient_str.insert(0, '-');
    else if (fPlus && n > 0)
        quotient_str.insert(0, '+');

    if (num_decimals <= 0)
        return quotient_str;

    if (cleanRemainderZeros) {
        QString cleanRemainder = remainder_str;
        for (int i = (remainder_str.length() - 1); i > 1; i--) {
            if (remainder_str.at(i) == QChar('0'))
                cleanRemainder = cleanRemainder.left(cleanRemainder.lastIndexOf("0"));
            else
                break;
        }
        return quotient_str + QString(".") + cleanRemainder;
    }

    return quotient_str + QString(".") + remainder_str;
}

QString BitcoinUnits::formatWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    return format(unit, amount, plussign, separators) + QString(" ") + name(unit);
}

QString BitcoinUnits::formatHtmlWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QString str(formatWithUnit(unit, amount, plussign, separators));
    str.replace(QChar(THIN_SP_CP), QString(COMMA_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}

QString BitcoinUnits::floorWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators, bool cleanRemainderZeros)
{
    QSettings settings;
    int digits = settings.value("digits").toInt();

    QString result = format(unit, amount, plussign, separators, cleanRemainderZeros);
    if (decimals(unit) > digits) {
        if (!cleanRemainderZeros) {
            result.chop(decimals(unit) - digits);
        } else {
            int lenght = result.mid(result.indexOf("."), result.length() - 1).length() - 1;
            if (lenght > digits)
                result.chop(lenght - digits);
        }
    }

    return result + QString(" ") + name(unit);
}

QString BitcoinUnits::floorHtmlWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators, bool cleanRemainderZeros)
{
    QString str(floorWithUnit(unit, amount, plussign, separators, cleanRemainderZeros));
    str.replace(QChar(THIN_SP_CP), QString(COMMA_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}

bool BitcoinUnits::parse(int unit, const QString& value, CAmount* val_out)
{
    if (!valid(unit) || value.isEmpty())
        return false;

    int num_decimals = decimals(unit);
    QStringList parts = removeSpaces(value).replace(",", ".").split(".");

    if (parts.size() > 2)
        return false;

    QString whole = parts[0];
    QString decimals;

    if (parts.size() > 1)
        decimals = parts[1];

    if (decimals.size() > num_decimals)
        return false;

    bool ok = false;
    QString str = whole + decimals.leftJustified(num_decimals, '0');

    if (str.size() > 18)
        return false;

    CAmount retvalue(str.toLongLong(&ok));
    if (val_out)
        *val_out = retvalue;

    return ok;
}

QString BitcoinUnits::getAmountColumnTitle(int unit)
{
    QString amountTitle = QObject::tr("Amount");
    if (BitcoinUnits::valid(unit))
        amountTitle += " (" + BitcoinUnits::name(unit) + ")";
    return amountTitle;
}

int BitcoinUnits::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return unitlist.size();
}

QVariant BitcoinUnits::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    if (row >= 0 && row < unitlist.size()) {
        Unit unit = unitlist.at(row);
        switch (role) {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return QVariant(name(unit));
        case Qt::ToolTipRole:
            return QVariant(description(unit));
        case UnitRole:
            return QVariant(static_cast<int>(unit));
        }
    }
    return QVariant();
}

CAmount BitcoinUnits::maxMoney()
{
    return Params().GetConsensus().nMaxMoneyOut;
}