#!/usr/bin/env python3
# Copyright (c) 2016-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test label RPCs.

RPCs tested are:
    - getaddressesbylabel
    - listaddressgroupings
    - setlabel
"""
from collections import defaultdict

from test_framework.test_framework import c_noteTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

class WalletlabelsTest(c_noteTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        node = self.nodes[0]
        assert_equal(len(node.listunspent()), 0)

        # Note each time we call generate, all generated coins go into
        # the same address, so we call twice to get two addresses w/50 each
        node.generate(1)
        node.generate(101)
        assert_equal(node.getbalance(), 500)

        # there should be 2 address groups
        # each with 1 address with a balance of 250 CNOTEs
        address_groups = node.listaddressgroupings()
        assert_equal(len(address_groups), 2)
        # the addresses aren't linked now, but will be after we send to the
        # common address
        linked_addresses = set()
        for address_group in address_groups:
            assert_equal(len(address_group), 1)
            assert_equal(len(address_group[0]), 2)
            assert_equal(address_group[0][1], 250)
            linked_addresses.add(address_group[0][0])

        # send 50 from each address to a third address not in this wallet
        # There's some fee that will come back to us when the miner reward
        # matures.
        node.settxfee(0)
        common_address = "y9B3dwrBGGs3yVkyEHm68Yn36Wp2Rt7Vtd"
        txid = node.sendmany("", {common_address: 100}, 1)
        tx_details = node.gettransaction(txid)
        fee = -tx_details['details'][0]['fee']
        # there should be 1 address group, with the previously
        # unlinked addresses now linked (they both have 0 balance)
        #address_groups = node.listaddressgroupings()
        #assert_equal(len(address_groups), 1)
        #assert_equal(len(address_groups[0]), 1)
        #assert_equal(set([a[0] for a in address_groups[0]]), linked_addresses)
        #assert_equal([a[1] for a in address_groups[0]], [0, 0])

        node.generate(1)

        # we want to reset so that the "" label has what's expected.
        # otherwise we're off by exactly the fee amount as that's mined
        # and matures in the next 100 blocks
        amount_to_send = 5.0

        # Create labels and make sure subsequent label API calls
        # recognize the label/address associations.
        labels = [Label(name) for name in ("a", "b", "c", "d", "e")]
        for label in labels:
            address = node.getnewaddress(label.name)
            label.add_receive_address(address)
            label.verify(node)

        # Check all labels are returned by listlabels.
        assert_equal(node.listlabels(), [label.name for label in labels])

        # Send a transaction to each label.
        for label in labels:
            node.sendtoaddress(label.addresses[0], amount_to_send)
            label.verify(node)

        # Check the amounts received.
        node.generate(1)
        for label in labels:
            assert_equal(
                node.getreceivedbyaddress(label.addresses[0]), amount_to_send)
            assert_equal(node.getreceivedbylabel(label.name), amount_to_send)

        for i, label in enumerate(labels):
            to_label = labels[(i + 1) % len(labels)]
            node.sendtoaddress(to_label.addresses[0], amount_to_send)
        node.generate(1)
        for label in labels:
            address = node.getnewaddress(label.name)
            label.add_receive_address(address)
            label.verify(node)
            assert_equal(node.getreceivedbylabel(label.name), 10)
            label.verify(node)
        node.generate(101)
        # expected_account_balances = {"": 26149.99985650}
        # for label in labels:
        #    expected_account_balances[label.name] = 0
        # if accounts_api:
        # assert_equal(node.listaccounts(), expected_account_balances)
        # assert_equal(node.getbalance(""), 5200)

        # Check that setlabel can assign a label to a new unused address.
        for label in labels:
            address = node.getnewaddress()
            node.setlabel(address, label.name)
            label.add_address(address)
            label.verify(node)
            assert_raises_rpc_error(-11, "No addresses with label", node.getaddressesbylabel, "")

        # Check that setlabel can assign a label to a new unused staking address.
        for label in labels:
            staking_address = node.getnewstakingaddress()
            node.setlabel(staking_address, label.name)
            label.add_address(staking_address)
            label.purpose[staking_address] = "coldstaking"
            label.verify(node)
            assert_raises_rpc_error(-11, "No addresses with label", node.getaddressesbylabel, "")

        # Check that addmultisigaddress can assign labels.
        for label in labels:
            addresses = []
            for x in range(10):
                addresses.append(node.getnewaddress())
            multisig_address = node.addmultisigaddress(5, addresses, label.name)
            label.add_address(multisig_address)
            label.purpose[multisig_address] = "send"
            label.verify(node)
            # node.generate(101)

        # Check that setlabel can change the label of an address from a
        # different label.
        change_label(node, labels[0].addresses[0], labels[0], labels[1])

        # Check that setlabel can set the label of an address already
        # in the label. This is a no-op.
        change_label(node, labels[2].addresses[0], labels[2], labels[2])

class Label:
    def __init__(self, name):
        # Label name
        self.name = name
        # Current receiving address associated with this label.
        self.receive_address = None
        # List of all addresses assigned with this label
        self.addresses = []
        # Map of address to address purpose
        self.purpose = defaultdict(lambda: "receive")

    def add_address(self, address):
        assert_equal(address not in self.addresses, True)
        self.addresses.append(address)

    def add_receive_address(self, address):
        self.add_address(address)

    def verify(self, node):
        if self.receive_address is not None:
            assert self.receive_address in self.addresses

        for address in self.addresses:
            assert_equal(
                node.getaddressinfo(address)['labels'][0],
                {"name": self.name,
                 "purpose": self.purpose[address]})
            assert_equal(node.getaddressinfo(address)['label'], self.name)

        assert_equal(
            node.getaddressesbylabel(self.name),
            {address: {"purpose": self.purpose[address]} for address in self.addresses})


def change_label(node, address, old_label, new_label):
    assert_equal(address in old_label.addresses, True)
    node.setlabel(address, new_label.name)

    old_label.addresses.remove(address)
    new_label.add_address(address)

    old_label.verify(node)
    new_label.verify(node)


if __name__ == '__main__':
    WalletlabelsTest().main()
