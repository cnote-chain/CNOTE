// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2021 The C_Note developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRANSACTIONTABLEMODEL_H
#define BITCOIN_QT_TRANSACTIONTABLEMODEL_H

#include "bitcoinunits.h"

#include <QAbstractTableModel>
#include <QFutureWatcher>
#include <QList>
#include <QStringList>

#include <memory>

namespace interfaces {
    class Handler;
}

class TransactionRecord;
class TransactionTablePriv;
class WalletModel;

class CWallet;

/** UI model for the transaction table of a wallet.
 */
class TransactionTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit TransactionTableModel(CWallet* wallet, WalletModel* parent = 0);
    ~TransactionTableModel();

    enum ColumnIndex {
        Status = 0,
        Watchonly = 1,
        Date = 2,
        Type = 3,
        ToAddress = 4,
        Amount = 5
    };

    /** Roles to get specific information from a transaction row.
        These are independent of column.
    */
    enum RoleIndex {
        /** Type of transaction */
        TypeRole = Qt::UserRole,
        /** Date and time this transaction was created */
        DateRole,
        /** Watch-only boolean */
        WatchonlyRole,
        /** Watch-only icon */
        WatchonlyDecorationRole,
        /** Address of transaction */
        AddressRole,
        /** Label of address related to transaction */
        LabelRole,
        /** Net amount of transaction */
        AmountRole,
        /** Transaction hash */
        TxHashRole,
        /** Is transaction confirmed? */
        ConfirmedRole,
        /** Formatted amount, without brackets when unconfirmed */
        FormattedAmountRole,
        /** Transaction status (TransactionRecord::Status) */
        StatusRole,
        /** Credit amount of transaction */
        ShieldedCreditAmountRole,
        /** Transaction size in bytes */
        SizeRole
    };

    int rowCount(const QModelIndex& parent) const;
    int columnCount(const QModelIndex& parent) const;
    int size() const;
    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
    bool processingQueuedTransactions() { return fProcessingQueuedTransactions; }

    // Pagination API. The view should call loadMore() when the user clicks
    // "Load More" and listen to loadStateChanged() to enable/disable the button.
    bool canLoadMore() const { return m_initialLoadDone && !m_loadingMore && size() < m_totalWalletTxCount; }
    int totalWalletTxCount() const { return m_totalWalletTxCount; }
    void loadMore(int additional = 100);

Q_SIGNALS:
    void txArrived(const QString& hash, const bool& isCoinStake, const bool isMNReward, const bool& isCSAnyType);
    /** Emitted when the initial load finishes or a loadMore batch is appended. */
    void loadStateChanged();

private:
    // Listeners
    std::unique_ptr<interfaces::Handler> m_handler_transaction_changed;
    std::unique_ptr<interfaces::Handler> m_handler_show_progress;

    CWallet* wallet{nullptr};
    WalletModel* walletModel{nullptr};
    QStringList columns{};
    TransactionTablePriv* priv{nullptr};
    bool fProcessingQueuedTransactions{false};

    // Async initial load: refreshWallet runs on a worker thread so the GUI
    // can paint immediately. Filled by the worker, swapped into priv on completion.
    QFutureWatcher<void>* m_loadWatcher{nullptr};
    QList<TransactionRecord> m_pendingRecords;
    qint64 m_pendingFirstLoadedTxTime{0};
    int m_pendingTotalWalletTxCount{0};
    bool m_initialLoadDone{false};

    // Pagination: how many wallet txs exist in total, vs how many we've loaded.
    int m_totalWalletTxCount{0};
    bool m_loadingMore{false};
    QFutureWatcher<void>* m_loadMoreWatcher{nullptr};

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    QString lookupAddress(const std::string& address, bool tooltip) const;
    QVariant addressColor(const TransactionRecord* wtx) const;
    QString formatTxStatus(const TransactionRecord* wtx) const;
    QString formatTxDate(const TransactionRecord* wtx) const;
    QString formatTxType(const TransactionRecord* wtx) const;
    QString formatTxToAddress(const TransactionRecord* wtx, bool tooltip) const;
    QString formatTxAmount(const TransactionRecord* wtx, bool showUnconfirmed = true, BitcoinUnits::SeparatorStyle separators = BitcoinUnits::separatorStandard) const;
    QString formatTooltip(const TransactionRecord* rec) const;
    QVariant txStatusDecoration(const TransactionRecord* wtx) const;
    QVariant txWatchonlyDecoration(const TransactionRecord* wtx) const;
    QVariant txAddressDecoration(const TransactionRecord* wtx) const;

public Q_SLOTS:
    /* New transaction, or transaction changed status */
    void updateTransaction(const QString& hash, int status, bool showTransaction);
    void updateConfirmations();
    void updateDisplayUnit();
    /** Updates the column title to "Amount (DisplayUnit)" and emits headerDataChanged() signal for table headers to react. */
    void updateAmountColumnTitle();
    /* Needed to update fProcessingQueuedTransactions through a QueuedConnection */
    void setProcessingQueuedTransactions(bool value) { fProcessingQueuedTransactions = value; }
    /* Re-run the initial async load. Invoked once the initial block download
     * completes, since per-tx updates are suppressed while syncing. */
    void reload();

private Q_SLOTS:
    void onInitialLoadFinished();
    void onLoadMoreFinished();
    void onReloadFinished();

public:
    friend class TransactionTablePriv;
};

#endif // BITCOIN_QT_TRANSACTIONTABLEMODEL_H
