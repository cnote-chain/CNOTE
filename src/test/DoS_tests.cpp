// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2021 The C_Note developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for denial-of-service detection/prevention code
//

#include "test/test_c_note.h"

#include "keystore.h"
#include "net_processing.h"
#include "net.h"
#include "pow.h"
#include "script/sign.h"
#include "serialize.h"
#include "util.h"
#include "validation.h"

#include <stdint.h>

#include <boost/test/unit_test.hpp>

// Tests this internal-to-validation.cpp method:
extern bool AddOrphanTx(const CTransactionRef& tx, NodeId peer);
extern void EraseOrphansFor(NodeId peer);
extern unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);
struct COrphanTx {
    CTransactionRef tx;
    NodeId fromPeer;
};
extern std::map<uint256, COrphanTx> mapOrphanTransactions;
extern std::map<uint256, std::set<uint256> > mapOrphanTransactionsByPrev;

CService ip(uint32_t i)
{
    struct in_addr s;
    s.s_addr = i;
    return CService(CNetAddr(s), Params().GetDefaultPort());
}

static NodeId id = 0;

BOOST_FIXTURE_TEST_SUITE(DoS_tests, TestingSetup)

void misbehave(NodeId id, int value) {
    LOCK(cs_main);
    Misbehaving(id, value); // Should get banned
}

BOOST_AUTO_TEST_CASE(DoS_banning)
{
    std::atomic<bool> interruptDummy(false);

    connman->ClearBanned();
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr1, 0, 0, "", true);
    dummyNode1.SetSendVersion(PROTOCOL_VERSION);
    GetNodeSignals().InitializeNode(&dummyNode1, *connman);
    dummyNode1.nVersion = 1;
    dummyNode1.fSuccessfullyConnected = true;
    misbehave(dummyNode1.GetId(), 100); // Should get banned
    SendMessages(&dummyNode1, *connman, interruptDummy);
    BOOST_CHECK(connman->IsBanned(addr1));
    BOOST_CHECK(!connman->IsBanned(ip(0xa0b0c001|0x0000ff00))); // Different IP, not banned

    CAddress addr2(ip(0xa0b0c002), NODE_NONE);
    CNode dummyNode2(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr2, 1, 1, "", true);
    dummyNode2.SetSendVersion(PROTOCOL_VERSION);
    GetNodeSignals().InitializeNode(&dummyNode2, *connman);
    dummyNode2.nVersion = 1;
    dummyNode2.fSuccessfullyConnected = true;
    misbehave(dummyNode2.GetId(), 50);
    SendMessages(&dummyNode2, *connman, interruptDummy);
    BOOST_CHECK(!connman->IsBanned(addr2)); // 2 not banned yet...
    BOOST_CHECK(connman->IsBanned(addr1));  // ... but 1 still should be
    misbehave(dummyNode2.GetId(), 50);
    SendMessages(&dummyNode2, *connman, interruptDummy);
    BOOST_CHECK(connman->IsBanned(addr2));
}

BOOST_AUTO_TEST_CASE(DoS_banscore)
{
    std::atomic<bool> interruptDummy(false);

    connman->ClearBanned();
    gArgs.ForceSetArg("-banscore", "111"); // because 11 is my favorite number
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr1, 3, 1, "", true);
    dummyNode1.SetSendVersion(PROTOCOL_VERSION);
    GetNodeSignals().InitializeNode(&dummyNode1, *connman);
    dummyNode1.nVersion = 1;
    dummyNode1.fSuccessfullyConnected = true;
    misbehave(dummyNode1.GetId(), 100);
    SendMessages(&dummyNode1, *connman, interruptDummy);
    BOOST_CHECK(!connman->IsBanned(addr1));
    misbehave(dummyNode1.GetId(), 10);
    SendMessages(&dummyNode1, *connman, interruptDummy);
    BOOST_CHECK(!connman->IsBanned(addr1));
    misbehave(dummyNode1.GetId(), 1);
    SendMessages(&dummyNode1, *connman, interruptDummy);
    BOOST_CHECK(connman->IsBanned(addr1));
    gArgs.ForceSetArg("-banscore", std::to_string(DEFAULT_BANSCORE_THRESHOLD));
}

BOOST_AUTO_TEST_CASE(DoS_bantime)
{
    std::atomic<bool> interruptDummy(false);

    connman->ClearBanned();
    int64_t nStartTime = GetTime();
    SetMockTime(nStartTime); // Overrides future calls to GetTime()

    CAddress addr(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr, 4, 4, "", true);
    dummyNode.SetSendVersion(PROTOCOL_VERSION);
    GetNodeSignals().InitializeNode(&dummyNode, *connman);
    dummyNode.nVersion = 1;
    dummyNode.fSuccessfullyConnected = true;

    misbehave(dummyNode.GetId(), 100);
    SendMessages(&dummyNode, *connman, interruptDummy);
    BOOST_CHECK(connman->IsBanned(addr));

    SetMockTime(nStartTime+60*60);
    BOOST_CHECK(connman->IsBanned(addr));

    SetMockTime(nStartTime+60*60*24+1);
    BOOST_CHECK(!connman->IsBanned(addr));
}

CTransactionRef RandomOrphan()
{
    std::map<uint256, COrphanTx>::iterator it;
    it = mapOrphanTransactions.lower_bound(InsecureRand256());
    if (it == mapOrphanTransactions.end())
        it = mapOrphanTransactions.begin();
    return it->second.tx;
}

BOOST_AUTO_TEST_CASE(DoS_mapOrphans)
{
    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    keystore.AddKey(key);

    // 50 orphan transactions:
    for (int i = 0; i < 50; i++)
    {
        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = InsecureRand256();
        tx.vin[0].scriptSig << OP_1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

        AddOrphanTx(MakeTransactionRef(tx), i);
    }

    // ... and 50 that depend on other orphans:
    for (int i = 0; i < 50; i++)
    {
        CTransactionRef txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = txPrev->GetHash();
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
        SignSignature(keystore, *txPrev, tx, 0, SIGHASH_ALL);

        AddOrphanTx(MakeTransactionRef(tx), i);
    }

    // This really-big orphan should be ignored:
    for (int i = 0; i < 10; i++)
    {
        CTransactionRef txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
        tx.vin.resize(500);
        for (unsigned int j = 0; j < tx.vin.size(); j++)
        {
            tx.vin[j].prevout.n = j;
            tx.vin[j].prevout.hash = txPrev->GetHash();
        }
        SignSignature(keystore, *txPrev, tx, 0, SIGHASH_ALL);
        // Re-use same signature for other inputs
        // (they don't have to be valid for this test)
        for (unsigned int j = 1; j < tx.vin.size(); j++)
            tx.vin[j].scriptSig = tx.vin[0].scriptSig;

        BOOST_CHECK(!AddOrphanTx(MakeTransactionRef(tx), i));
    }

    // Test EraseOrphansFor:
    for (NodeId i = 0; i < 3; i++)
    {
        size_t sizeBefore = mapOrphanTransactions.size();
        EraseOrphansFor(i);
        BOOST_CHECK(mapOrphanTransactions.size() < sizeBefore);
    }

    // Test LimitOrphanTxSize() function:
    LimitOrphanTxSize(40);
    BOOST_CHECK(mapOrphanTransactions.size() <= 40);
    LimitOrphanTxSize(10);
    BOOST_CHECK(mapOrphanTransactions.size() <= 10);
    LimitOrphanTxSize(0);
    BOOST_CHECK(mapOrphanTransactions.empty());
    BOOST_CHECK(mapOrphanTransactionsByPrev.empty());
}

BOOST_AUTO_TEST_SUITE_END()
