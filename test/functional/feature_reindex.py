#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test running bitcoind with -reindex and -reindex-chainstate options.

- Start a single node and generate 3 blocks.
- Stop the node and restart it with -reindex. Verify that the node has reindexed up to block 3.
- Stop the node and restart it with -reindex-chainstate. Verify that the node has reindexed up to block 3.
"""

from test_framework.test_framework import c_noteTestFramework
from test_framework.util import assert_equal
import time

class ReindexTest(c_noteTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def reindex(self):
        self.nodes[0].generate(3)
        blockcount = self.nodes[0].getblockcount()
        self.stop_nodes()
        extra_args = [["-reindex", "-checkblockindex=1"]]
        self.start_nodes(extra_args)
        assert_equal(self.nodes[0].getblockcount(), blockcount)  # start_node is blocking on reindex
        self.log.info("Success")

    def run_test(self):
        self.reindex()

if __name__ == '__main__':
    ReindexTest().main()
