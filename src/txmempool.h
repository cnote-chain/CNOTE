// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2021 The C_Note developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXMEMPOOL_H
#define BITCOIN_TXMEMPOOL_H

#include <list>
#include <memory>
#include <set>

#include "amount.h"
#include "coins.h"
#include "indirectmap.h"
#include "policy/feerate.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "random.h"

#include "boost/multi_index_container.hpp"
#include "boost/multi_index/ordered_index.hpp"
#include "boost/multi_index/hashed_index.hpp"

#include <boost/signals2/signal.hpp>

class CAutoFile;

inline double AllowFreeThreshold()
{
    return COIN * 1440 / 250;
}

inline bool AllowFree(double dPriority)
{
    // Large (in bytes) low-priority (new, small-coin) transactions
    // need a fee.
    return dPriority > AllowFreeThreshold();
}


/** Fake height value used in Coin to signify they are only in the memory pool (since 0.8) */
static const uint32_t MEMPOOL_HEIGHT = 0x7FFFFFFF;

class CTxMemPool;

/** \class CTxMemPoolEntry
 *
 * CTxMemPoolEntry stores data about the correponding transaction, as well
 * as data about all in-mempool transactions that depend on the transaction
 * ("descendant" transactions).
 *
 * When a new entry is added to the mempool, we update the descendant state
 * (nCountWithDescendants, nSizeWithDescendants, and nModFeesWithDescendants) for
 * all ancestors of the newly added transaction.
 *
 * If updating the descendant state is skipped, we can mark the entry as
 * "dirty", and set nSizeWithDescendants/nModFeesWithDescendants to equal nTxSize/
 * nFee+feeDelta. (This can potentially happen during a reorg, where we limit the
 * amount of work we're willing to do to avoid consuming too much CPU.)
 *
 */
class CTxMemPoolEntry
{
private:
    CTransactionRef tx;
    CAmount nFee;         //! Cached to avoid expensive parent-transaction lookups
    size_t nTxSize;       //! ... and avoid recomputing tx size
    size_t nModSize;      //! ... and modified size for priority
    size_t nUsageSize;    //! ... and total memory usage
    CFeeRate feeRate;     //! ... and fee per kB
    bool m_isShielded{false}; //! ... and checking if it contains shielded spends/outputs
    int64_t nTime;        //! Local time when entering the mempool
    double entryPriority;     //! Priority when entering the mempool
    unsigned int entryHeight; //! Chain height when entering the mempool
    bool hadNoDependencies; //! Not dependent on any other txs when it entered the mempool
    CAmount inChainInputValue; //! Sum of all txin values that are already in blockchain
    bool spendsCoinbaseOrCoinstake; //! keep track of transactions that spend a coinbase or a coinstake
    unsigned int sigOpCount; //! Legacy sig ops plus P2SH sig op count
    int64_t feeDelta; //! Used for determining the priority of the transaction for mining in a block

    // Information about descendants of this transaction that are in the
    // mempool; if we remove this transaction we must remove all of these
    // descendants as well.  if nCountWithDescendants is 0, treat this entry as
    // dirty, and nSizeWithDescendants and nModFeesWithDescendants will not be
    // correct.
    uint64_t nCountWithDescendants; //! number of descendant transactions
    uint64_t nSizeWithDescendants;  //! ... and size
    CAmount nModFeesWithDescendants;  //! ... and total fees (all including us)

    // Analogous statistics for ancestor transactions
    uint64_t nCountWithAncestors;
    uint64_t nSizeWithAncestors;
    CAmount nModFeesWithAncestors;
    unsigned int nSigOpCountWithAncestors;

public:
    CTxMemPoolEntry(const CTransactionRef& _tx, const CAmount& _nFee,
            int64_t _nTime, double _entryPriority, unsigned int _entryHeight,
            bool poolHasNoInputsOf, CAmount _inChainInputValue, bool _spendsCoinbaseOrCoinstake,
            unsigned int nSigOps);

    const CTransaction& GetTx() const { return *this->tx; }
    std::shared_ptr<const CTransaction> GetSharedTx() const { return this->tx; }
    /**
     * Fast calculation of lower bound of current priority as update
     * from entry priority. Only inputs that were originally in-chain will age.
     */
    double GetPriority(unsigned int currentHeight) const;
    const CAmount& GetFee() const { return nFee; }
    size_t GetTxSize() const { return nTxSize; }
    int64_t GetTime() const { return nTime; }
    unsigned int GetHeight() const { return entryHeight; }
    bool IsShielded() const { return m_isShielded; }
    bool WasClearAtEntry() const { return hadNoDependencies; }
    unsigned int GetSigOpCount() const { return sigOpCount; }
    int64_t GetModifiedFee() const { return nFee + feeDelta; }
    size_t DynamicMemoryUsage() const { return nUsageSize; }

    // Adjusts the descendant state, if this entry is not dirty.
    void UpdateDescendantState(int64_t modifySize, CAmount modifyFee, int64_t modifyCount);
    // Adjusts the ancestor state
    void UpdateAncestorState(int64_t modifySize, CAmount modifyFee, int64_t modifyCount, int modifySigOps);
    // Updates the fee delta used for mining priority score, and the
    // modified fees with descendants.
    void UpdateFeeDelta(int64_t feeDelta);

    uint64_t GetCountWithDescendants() const { return nCountWithDescendants; }
    uint64_t GetSizeWithDescendants() const { return nSizeWithDescendants; }
    CAmount GetModFeesWithDescendants() const { return nModFeesWithDescendants; }

    bool GetSpendsCoinbaseOrCoinstake() const { return spendsCoinbaseOrCoinstake; }

    uint64_t GetCountWithAncestors() const { return nCountWithAncestors; }
    uint64_t GetSizeWithAncestors() const { return nSizeWithAncestors; }
    CAmount GetModFeesWithAncestors() const { return nModFeesWithAncestors; }
    unsigned int GetSigOpCountWithAncestors() const { return nSigOpCountWithAncestors; }
};

// Helpers for modifying CTxMemPool::mapTx, which is a boost multi_index.
struct update_descendant_state
{
    update_descendant_state(int64_t _modifySize, CAmount _modifyFee, int64_t _modifyCount) :
        modifySize(_modifySize), modifyFee(_modifyFee), modifyCount(_modifyCount)
    {}

    void operator() (CTxMemPoolEntry &e)
        { e.UpdateDescendantState(modifySize, modifyFee, modifyCount); }

    private:
        int64_t modifySize;
        CAmount modifyFee;
        int64_t modifyCount;
};

struct update_ancestor_state
{
    update_ancestor_state(int64_t _modifySize, CAmount _modifyFee, int64_t _modifyCount, int _modifySigOps) :
            modifySize(_modifySize), modifyFee(_modifyFee), modifyCount(_modifyCount), modifySigOps(_modifySigOps)
    {}

    void operator() (CTxMemPoolEntry &e)
    { e.UpdateAncestorState(modifySize, modifyFee, modifyCount, modifySigOps); }

private:
    int64_t modifySize;
    CAmount modifyFee;
    int64_t modifyCount;
    int modifySigOps;
};

struct update_fee_delta
{
    update_fee_delta(int64_t _feeDelta) : feeDelta(_feeDelta) { }

    void operator() (CTxMemPoolEntry &e) { e.UpdateFeeDelta(feeDelta); }

private:
    int64_t feeDelta;
};

// extracts a TxMemPoolEntry's transaction hash
struct mempoolentry_txid
{
    typedef uint256 result_type;
    result_type operator() (const CTxMemPoolEntry &entry) const
    {
        return entry.GetTx().GetHash();
    }
};

/** \class CompareTxMemPoolEntryByDescendantScore
 *
 *  Sort an entry by max(score/size of entry's tx, score/size with all descendants).
 */
class CompareTxMemPoolEntryByDescendantScore
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        bool fUseADescendants = UseDescendantScore(a);
        bool fUseBDescendants = UseDescendantScore(b);

        double aModFee = fUseADescendants ? a.GetModFeesWithDescendants() : a.GetModifiedFee();
        double aSize = fUseADescendants ? a.GetSizeWithDescendants() : a.GetTxSize();

        double bModFee = fUseBDescendants ? b.GetModFeesWithDescendants() : b.GetModifiedFee();
        double bSize = fUseBDescendants ? b.GetSizeWithDescendants() : b.GetTxSize();

        // Avoid division by rewriting (a/b > c/d) as (a*d > c*b).
        double f1 = aModFee * bSize;
        double f2 = aSize * bModFee;

        if (f1 == f2) {
            return a.GetTime() >= b.GetTime();
        }
        return f1 < f2;
    }

    // Calculate which score to use for an entry (avoiding division).
    bool UseDescendantScore(const CTxMemPoolEntry &a) const
    {
        double f1 = (double)a.GetModifiedFee() * a.GetSizeWithDescendants();
        double f2 = (double)a.GetModFeesWithDescendants() * a.GetTxSize();
        return f2 > f1;
    }
};

/** \class CompareTxMemPoolEntryByScore
 *
 *  Sort by score of entry ((fee+delta)/size) in descending order
 */
class CompareTxMemPoolEntryByScore
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        double f1 = (double)a.GetModifiedFee() * b.GetTxSize();
        double f2 = (double)b.GetModifiedFee() * a.GetTxSize();
        if (f1 == f2) {
            return b.GetTx().GetHash() < a.GetTx().GetHash();
        }
        return f1 > f2;
    }
};

class CompareTxMemPoolEntryByEntryTime
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        return a.GetTime() < b.GetTime();
    }
};

class CompareTxMemPoolEntryByAncestorFee
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        double aFees = a.GetModFeesWithAncestors();
        double aSize = a.GetSizeWithAncestors();

        double bFees = b.GetModFeesWithAncestors();
        double bSize = b.GetSizeWithAncestors();

        // Avoid division by rewriting (a/b > c/d) as (a*d > c*b).
        double f1 = aFees * bSize;
        double f2 = aSize * bFees;

        if (f1 == f2) {
            return a.GetTx().GetHash() < b.GetTx().GetHash();
        }

        return f1 > f2;
    }
};

// Multi_index tag names
struct descendant_score {};
struct entry_time {};
struct mining_score {};
struct ancestor_score {};

class CBlockPolicyEstimator;

/**
 * Information about a mempool transaction.
 */
struct TxMempoolInfo
{
    /** The transaction itself */
    CTransactionRef tx;

    /** Time the transaction entered the mempool. */
    int64_t nTime;

    /** Feerate of the transaction. */
    CFeeRate feeRate;

    /** The fee delta. */
    int64_t nFeeDelta;
};

/** Reason why a transaction was removed from the mempool,
 * this is passed to the notification signal.
 */
enum class MemPoolRemovalReason {
    UNKNOWN = 0, //! Manually removed or unknown reason
    EXPIRY,      //! Expired from mempool
    SIZELIMIT,   //! Removed in size limiting
    REORG,       //! Removed for reorganization
    BLOCK,       //! Removed for block
    CONFLICT,    //! Removed for conflict with in-block transaction
    REPLACED     //! Removed for replacement
};

/**
 * CTxMemPool stores valid-according-to-the-current-best-chain
 * transactions that may be included in the next block.
 *
 * Transactions are added when they are seen on the network
 * (or created by the local node), but not all transactions seen
 * are added to the pool: if a new transaction double-spends
 * an input of a transaction in the pool, it is dropped,
 * as are non-standard transactions.
 *
 * CTxMemPool::mapTx, and CTxMemPoolEntry bookkeeping:
 *
 * mapTx is a boost::multi_index that sorts the mempool on 4 criteria:
 * - transaction hash
 * - feerate [we use max(feerate of tx, feerate of tx with all descendants)]
 * - time in mempool
 * - mining score (feerate modified by any fee deltas from PrioritiseTransaction)

 *
 * Note: the term "descendant" refers to in-mempool transactions that depend on
 * this one, while "ancestor" refers to in-mempool transactions that a given
 * transaction depends on.
 *
 * In order for the feerate sort to remain correct, we must update transactions
 * in the mempool when new descendants arrive.  To facilitate this, we track
 * the set of in-mempool direct parents and direct children in mapLinks.  Within
 * each CTxMemPoolEntry, we track the size and fees of all descendants.
 *
 * Usually when a new transaction is added to the mempool, it has no in-mempool
 * children (because any such children would be an orphan).  So in
 * addUnchecked(), we:
 * - update a new entry's setMemPoolParents to include all in-mempool parents
 * - update the new entry's direct parents to include the new tx as a child
 * - update all ancestors of the transaction to include the new tx's size/fee
 *
 * When a transaction is removed from the mempool, we must:
 * - update all in-mempool parents to not track the tx in setMemPoolChildren
 * - update all ancestors to not include the tx's size/fees in descendant state
 * - update all in-mempool children to not include it as a parent
 *
 * These happen in UpdateForRemoveFromMempool().  (Note that when removing a
 * transaction along with its descendants, we must calculate that set of
 * transactions to be removed before doing the removal, or else the mempool can
 * be in an inconsistent state where it's impossible to walk the ancestors of
 * a transaction.)
 *
 * In the event of a reorg, the assumption that a newly added tx has no
 * in-mempool children is false.  In particular, the mempool is in an
 * inconsistent state while new transactions are being added, because there may
 * be descendant transactions of a tx coming from a disconnected block that are
 * unreachable from just looking at transactions in the mempool (the linking
 * transactions may also be in the disconnected block, waiting to be added).
 * Because of this, there's not much benefit in trying to search for in-mempool
 * children in addUnchecked().  Instead, in the special case of transactions
 * being added from a disconnected block, we require the caller to clean up the
 * state, to account for in-mempool, out-of-block descendants for all the
 * in-block transactions by calling UpdateTransactionsFromBlock().  Note that
 * until this is called, the mempool state is not consistent, and in particular
 * mapLinks may not be correct (and therefore functions like
 * CalculateMemPoolAncestors() and CalculateDescendants() that rely
 * on them to walk the mempool are not generally safe to use).
 *
 * Computational limits:
 *
 * Updating all in-mempool ancestors of a newly added transaction can be slow,
 * if no bound exists on how many in-mempool ancestors there may be.
 * CalculateMemPoolAncestors() takes configurable limits that are designed to
 * prevent these calculations from being too CPU intensive.
 *
 * Adding transactions from a disconnected block can be very time consuming,
 * because we don't have a way to limit the number of in-mempool descendants.
 * To bound CPU processing, we limit the amount of work we're willing to do
 * to properly update the descendant information for a tx being added from
 * a disconnected block.  If we would exceed the limit, then we instead mark
 * the entry as "dirty", and set the feerate for sorting purposes to be equal
 * the feerate of the transaction without any descendants.
 *
 */
class CTxMemPool
{
private:
    uint32_t nCheckFrequency; //! Value n means that n times in 2^32 we check.
    unsigned int nTransactionsUpdated;
    CBlockPolicyEstimator* minerPolicyEstimator;

    CFeeRate minRelayFee; //! Passed to constructor to avoid dependency on main
    uint64_t totalTxSize; //! sum of all mempool tx' byte sizes
    uint64_t cachedInnerUsage; //! sum of dynamic memory usage of all the map elements (NOT the maps themselves)

    CFeeRate minReasonableRelayFee;

    mutable int64_t lastRollingFeeUpdate;
    mutable bool blockSinceLastRollingFeeBump;
    mutable double rollingMinimumFeeRate; //! minimum fee to get into the pool, decreases exponentially

    void trackPackageRemoved(const CFeeRate& rate);

    // Shielded txes
    std::map<uint256, CTransactionRef> mapSaplingNullifiers;
    void checkNullifiers() const;

    bool m_is_loaded GUARDED_BY(cs){false};

public:

    static const int ROLLING_FEE_HALFLIFE = 60 * 60 * 12; // public only for testing

    typedef boost::multi_index_container<
        CTxMemPoolEntry,
        boost::multi_index::indexed_by<
            // sorted by txid
            boost::multi_index::hashed_unique<mempoolentry_txid, SaltedIdHasher>,
            // sorted by fee rate
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<descendant_score>,
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByDescendantScore
            >,
            // sorted by entry time
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<entry_time>,
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByEntryTime
            >,
            // sorted by score (for mining prioritization)
            boost::multi_index::ordered_unique<
                boost::multi_index::tag<mining_score>,
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByScore
            >,
            // sorted by fee rate with ancestors
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<ancestor_score>,
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByAncestorFee
            >
        >
    > indexed_transaction_set;

    /**
     * This mutex needs to be locked when accessing `mapTx` or other members
     * that are guarded by it.
     *
     * @par Consistency guarantees
     *
     * By design, it is guaranteed that:
     *
     * 1. Locking both `cs_main` and `mempool.cs` will give a view of mempool
     *    that is consistent with current chain tip (`::ChainActive()` and
     *    `CoinsTip()`) and is fully populated. Fully populated means that if the
     *    current active chain is missing transactions that were present in a
     *    previously active chain, all the missing transactions will have been
     *    re-added to the mempool and should be present if they meet size and
     *    consistency constraints.
     *
     * 2. Locking `mempool.cs` without `cs_main` will give a view of a mempool
     *    consistent with some chain that was active since `cs_main` was last
     *    locked, and that is fully populated as described above. It is ok for
     *    code that only needs to query or remove transactions from the mempool
     *    to lock just `mempool.cs` without `cs_main`.
     *
     * To provide these guarantees, it is necessary to lock both `cs_main` and
     * `mempool.cs` whenever adding transactions to the mempool and whenever
     * changing the chain tip. It's necessary to keep both mutexes locked until
     * the mempool is consistent with the new chain tip and fully populated.
     */
    mutable RecursiveMutex cs;
    indexed_transaction_set mapTx;
    typedef indexed_transaction_set::nth_index<0>::type::iterator txiter;
    struct CompareIteratorByHash {
        bool operator()(const txiter &a, const txiter &b) const {
            return a->GetTx().GetHash() < b->GetTx().GetHash();
        }
    };
    typedef std::set<txiter, CompareIteratorByHash> setEntries;

    const setEntries & GetMemPoolParents(txiter entry) const;
    const setEntries & GetMemPoolChildren(txiter entry) const;
private:
    typedef std::map<txiter, setEntries, CompareIteratorByHash> cacheMap;

    struct TxLinks {
        setEntries parents;
        setEntries children;
    };

    typedef std::map<txiter, TxLinks, CompareIteratorByHash> txlinksMap;
    txlinksMap mapLinks;

    void UpdateParent(txiter entry, txiter parent, bool add);
    void UpdateChild(txiter entry, txiter child, bool add);

    std::vector<indexed_transaction_set::const_iterator> GetSortedDepthAndScore() const;

public:
    indirectmap<COutPoint, CTransactionRef> mapNextTx;
    std::map<uint256, std::pair<double, CAmount> > mapDeltas;

    /** Create a new CTxMemPool.
     *  minReasonableRelayFee should be a feerate which is, roughly, somewhere
     *  around what it "costs" to relay a transaction around the network and
     *  below which we would reasonably say a transaction has 0-effective-fee.
     */
    CTxMemPool(const CFeeRate& _minReasonableRelayFee);
    ~CTxMemPool();

    /**
     * If sanity-checking is turned on, check makes sure the pool is
     * consistent (does not contain two transactions that spend the same inputs,
     * all inputs are in the mapNextTx array). If sanity-checking is turned off,
     * check does nothing.
     */
    void check(const CCoinsViewCache *pcoins) const;
    void setSanityCheck(double dFrequency = 1.0) { nCheckFrequency = dFrequency * 4294967295.0; }

    // addUnchecked must updated state for all ancestors of a given transaction,
    // to track size/count of descendant transactions.  First version of
    // addUnchecked can be used to have it call CalculateMemPoolAncestors(), and
    // then invoke the second version.
    // Note that addUnchecked is ONLY called from ATMP outside of tests
    // and any other callers may break wallet's in-mempool tracking (due to
    // lack of CValidationInterface::TransactionAddedToMempool callbacks).
    bool addUnchecked(const uint256& hash, const CTxMemPoolEntry& entry, bool fCurrentEstimate = true);
    bool addUnchecked(const uint256& hash, const CTxMemPoolEntry &entry, setEntries &setAncestors, bool fCurrentEstimate = true);

    void removeRecursive(const CTransaction& tx, MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN);
    void removeForReorg(const CCoinsViewCache* pcoins, unsigned int nMemPoolHeight, int flags);
    void removeWithAnchor(const uint256& invalidRoot);
    void removeConflicts(const CTransaction& tx);
    void removeForBlock(const std::vector<CTransactionRef>& vtx, unsigned int nBlockHeight,
                        bool fCurrentEstimate = true);

    void clear();
    void _clear();  // lock-free
    bool CompareDepthAndScore(const uint256& hasha, const uint256& hashb);
    void queryHashes(std::vector<uint256>& vtxid);
    void getTransactions(std::set<uint256>& setTxid);
    bool isSpent(const COutPoint& outpoint);
    unsigned int GetTransactionsUpdated() const;
    void AddTransactionsUpdated(unsigned int n);
    /**
     * Check that none of this transactions inputs are in the mempool, and thus
     * the tx is not dependent on other mempool transactions to be included in a block.
     */
    bool HasNoInputsOf(const CTransaction& tx) const;

    /** Affect CreateNewBlock prioritisation of transactions */
    void PrioritiseTransaction(const uint256 hash, const std::string strHash, double dPriorityDelta, const CAmount& nFeeDelta);
    void ApplyDeltas(const uint256 hash, double& dPriorityDelta, CAmount& nFeeDelta) const;
    void ClearPrioritisation(const uint256 hash);

    bool nullifierExists(const uint256& nullifier) const;

    /** Remove a set of transactions from the mempool.
     *  If a transaction is in this set, then all in-mempool descendants must
     *  also be in the set, unless this transaction is being removed for being
     *  in a block.
     *  Set updateDescendants to true when removing a tx that was in a block, so
     *  that any in-mempool descendants have their ancestor state updated.
     */
    void RemoveStaged(setEntries &stage, bool updateDescendants, MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN);

    /** When adding transactions from a disconnected block back to the mempool,
     *  new mempool entries may have children in the mempool (which is generally
     *  not the case when otherwise adding transactions).
     *  UpdateTransactionsFromBlock() will find child transactions and update the
     *  descendant state for each transaction in hashesToUpdate (excluding any
     *  child transactions present in hashesToUpdate, which are already accounted
     *  for).  Note: hashesToUpdate should be the set of transactions from the
     *  disconnected block that have been accepted back into the mempool.
     */
    void UpdateTransactionsFromBlock(const std::vector<uint256> &hashesToUpdate);

    /** Try to calculate all in-mempool ancestors of entry.
     *  (these are all calculated including the tx itself)
     *  limitAncestorCount = max number of ancestors
     *  limitAncestorSize = max size of ancestors
     *  limitDescendantCount = max number of descendants any ancestor can have
     *  limitDescendantSize = max size of descendants any ancestor can have
     *  errString = populated with error reason if any limits are hit
     *  fSearchForParents = whether to search a tx's vin for in-mempool parents, or
     *    look up parents from mapLinks. Must be true for entries not in the mempool
     */
    bool CalculateMemPoolAncestors(const CTxMemPoolEntry &entry, setEntries &setAncestors, uint64_t limitAncestorCount, uint64_t limitAncestorSize, uint64_t limitDescendantCount, uint64_t limitDescendantSize, std::string &errString, bool fSearchForParents = true) const;

    /** The minimum fee to get into the mempool, which may itself not be enough
     *  for larger-sized transactions.
     *  The minReasonableRelayFee constructor arg is used to bound the time it
     *  takes the fee rate to go back down all the way to 0. When the feerate
     *  would otherwise be half of this, it is set to 0 instead.
     */
    CFeeRate GetMinFee(size_t sizelimit) const;

    /** Remove transactions from the mempool until its dynamic size is <= sizelimit.
      *  pvNoSpendsRemaining, if set, will be populated with the list of transactions
      *  which are not in mempool which no longer have any spends in this mempool.
      */
    void TrimToSize(size_t sizelimit, std::vector<COutPoint>* pvNoSpendsRemaining = nullptr);

    /** Expire all transaction (and their dependencies) in the mempool older than time. Return the number of removed transactions. */
    int Expire(int64_t time);

    /** @returns true if the mempool is fully loaded */
    bool IsLoaded() const;

    /** Sets the current loaded state */
    void SetIsLoaded(bool loaded);

    unsigned long size() const
    {
        LOCK(cs);
        return mapTx.size();
    }
    uint64_t GetTotalTxSize()
    {
        LOCK(cs);
        return totalTxSize;
    }

    bool exists(uint256 hash) const
    {
        LOCK(cs);
        return (mapTx.count(hash) != 0);
    }

    bool exists(const COutPoint& outpoint) const
    {
        LOCK(cs);
        auto it = mapTx.find(outpoint.hash);
        return (it != mapTx.end() && outpoint.n < it->GetTx().vout.size());
    }

    CTransactionRef get(const uint256& hash) const;
    TxMempoolInfo info(const uint256& hash) const;
    std::vector<TxMempoolInfo> infoAll() const;

    /** Estimate fee rate needed to get into the next nBlocks
     *  If no answer can be given at nBlocks, return an estimate
     *  at the lowest number of blocks where one can be given
     */
    CFeeRate estimateSmartFee(int nBlocks, int *answerFoundAtBlocks = NULL) const;

    /** Estimate fee rate needed to get into the next nBlocks */
    CFeeRate estimateFee(int nBlocks) const;

    /** Estimate priority needed to get into the next nBlocks
     *  If no answer can be given at nBlocks, return an estimate
     *  at the lowest number of blocks where one can be given
     */
    double estimateSmartPriority(int nBlocks, int *answerFoundAtBlocks = NULL) const;

    /** Estimate priority needed to get into the next nBlocks */
    double estimatePriority(int nBlocks) const;

    /** Write/Read estimates to disk */
    bool WriteFeeEstimates(CAutoFile& fileout) const;
    bool ReadFeeEstimates(CAutoFile& filein);

    size_t DynamicMemoryUsage() const;

    boost::signals2::signal<void (CTransactionRef)> NotifyEntryAdded;
    boost::signals2::signal<void (CTransactionRef, MemPoolRemovalReason)> NotifyEntryRemoved;

private:
    /** UpdateForDescendants is used by UpdateTransactionsFromBlock to update
     *  the descendants for a single transaction that has been added to the
     *  mempool but may have child transactions in the mempool, eg during a
     *  chain reorg.  setExclude is the set of descendant transactions in the
     *  mempool that must not be accounted for (because any descendants in
     *  setExclude were added to the mempool after the transaction being
     *  updated and hence their state is already reflected in the parent
     *  state).
     *
     *  cachedDescendants will be updated with the descendants of the transaction
     *  being updated, so that future invocations don't need to walk the
     *  same transaction again, if encountered in another transaction chain.
     */
    void UpdateForDescendants(txiter updateIt,
            cacheMap &cachedDescendants,
            const std::set<uint256> &setExclude);
    /** Update ancestors of hash to add/remove it as a descendant transaction. */
    void UpdateAncestorsOf(bool add, txiter hash, setEntries &setAncestors);
    /** Set ancestor state for an entry */
    void UpdateEntryForAncestors(txiter it, const setEntries &setAncestors);
    /** For each transaction being removed, update ancestors and any direct children.
      * If updateDescendants is true, then also update in-mempool descendants'
      * ancestor state. */
    void UpdateForRemoveFromMempool(const setEntries &entriesToRemove, bool updateDescendants);
    /** Sever link between specified transaction and direct children. */
    void UpdateChildrenForRemoval(txiter entry);
    /** Populate setDescendants with all in-mempool descendants of hash.
     *  Assumes that setDescendants includes all in-mempool descendants of anything
     *  already in it.  */
    void CalculateDescendants(txiter it, setEntries &setDescendants);

    /** Before calling removeUnchecked for a given transaction,
     *  UpdateForRemoveFromMempool must be called on the entire (dependent) set
     *  of transactions being removed at the same time.  We use each
     *  CTxMemPoolEntry's setMemPoolParents in order to walk ancestors of a
     *  given transaction that is removed, so we can't remove intermediate
     *  transactions in a chain before we've updated all the state for the
     *  removal.
     */
    void removeUnchecked(txiter entry, MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN);
};

/** 
 * CCoinsView that brings transactions from a memorypool into view.
 * It does not check for spendings by memory pool transactions.
 */
class CCoinsViewMemPool : public CCoinsViewBacked
{
protected:
    CTxMemPool& mempool;

public:
    CCoinsViewMemPool(CCoinsView* baseIn, CTxMemPool& mempoolIn);
    bool GetCoin(const COutPoint& outpoint, Coin& coin) const;
    bool HaveCoin(const COutPoint& outpoint) const;
    bool GetNullifier(const uint256& nullifier) const;
};

// We want to sort transactions by coin age priority
typedef std::pair<double, CTxMemPool::txiter> TxCoinAgePriority;

struct TxCoinAgePriorityCompare
{
    bool operator()(const TxCoinAgePriority& a, const TxCoinAgePriority& b)
    {
        if (a.first == b.first)
            return CompareTxMemPoolEntryByScore()(*(b.second), *(a.second)); //Reverse order to make sort less than
        return a.first < b.first;
    }
};

#endif // BITCOIN_TXMEMPOOL_H
