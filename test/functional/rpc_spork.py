#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Copyright (c) 2020, 2026 The SchillingCoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
# -*- coding: utf-8 -*-

# NOTE:
# This test previously validated SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT.
#
# SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT has been permanently removed from SCH.
# Its former behavior — enforcing the use of the stable masternode set for
# payment and validation — is now statically hard‑coded directly into the SCH
# consensus logic wherever it was previously required. Because SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT
# no longer exists as a dynamic RPC‑controlled feature, this test has been retired.
#
# The test is intentionally stubbed to preserve the functional test suite
# structure without implying that SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT is still
# available or should be reintroduced. No spork RPC calls are performed.

from test_framework.test_framework import SchillingcoinTestFramework


class SchillingCoin_RPCSporkTest(SchillingcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[]] * self.num_nodes

    def setup_chain(self):
        # Start with clean chain
        self._initialize_chain_clean()
        self.enable_mocktime()

    def log_title(self):
        title = "*** Starting %s (retired test) ***" % self.__class__.__name__
        underline = "-" * len(title)
        description = (
            "This test has been retired because "
            "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT no longer exists."
        )
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, description)

    def run_test(self):
        # Explicitly mark this test as retired and skip all logic.
        self.log_title()
        self.log.warning(
            "SKIPPED: SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT has been permanently removed."
        )
        self.log.warning(
            "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT behavior is now statically enforced "
            "in SCH consensus code."
        )
        self.log.warning("No RPC spork checks will be performed.")
        return


if __name__ == '__main__':
    SchillingCoin_RPCSporkTest().main()