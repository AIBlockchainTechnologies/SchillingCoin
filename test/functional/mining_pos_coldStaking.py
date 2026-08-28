#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Copyright (c) 2020, 2026 The SchillingCoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
# -*- coding: utf-8 -*-

# Note: SPORK_17_COLDSTAKING_ENFORCEMENT has been removed from runtime.
#       This test assumes cold staking is permanently enabled.

from io import BytesIO
from time import sleep

from test_framework.messages import CTransaction, CTxIn, CTxOut, COIN, COutPoint
from test_framework.mininode import network_thread_start
from test_framework.schillingcoin_node import SchillingcoinTestNode
from test_framework.script import CScript, OP_CHECKSIG
from test_framework.test_framework import SchillingcoinTestFramework
from test_framework.util import connect_nodes_bi, p2p_port, bytes_to_hex_str, set_node_times, \
    assert_equal, assert_greater_than, sync_blocks, sync_mempools, assert_raises_rpc_error

# filter utxos based on first 5 bytes of scriptPubKey (P2CS prefix)
def getDelegatedUtxos(utxos):
    delegated_prefix = '76a97b63d1'  # hex prefix for P2CS scripts used in tests
    return [
        x for x in utxos
        if isinstance(x, dict)
        and 'scriptPubKey' in x
        and isinstance(x['scriptPubKey'], str)
        and (lambda spk: len(spk) >= len(delegated_prefix) and spk[:len(delegated_prefix)].lower() == delegated_prefix)(x['scriptPubKey'].strip())
    ]

class SchillingCoin_ColdStakingTest(SchillingcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 3
        # Use a list comprehension so each node gets its own args list
        self.extra_args = [[] for _ in range(self.num_nodes)]
        # SPORK_17 removed; no spork key required

    def setup_chain(self):
        # Start with PoW cache: 200 blocks
        self._initialize_chain()
        self.enable_mocktime()

    def setup_network(self):
        ''' Can't rely on syncing all the nodes when staking=1
        '''
        self.setup_nodes()
        for i in range(self.num_nodes - 1):
            for j in range(i+1, self.num_nodes):
                connect_nodes_bi(self.nodes, i, j)

    def init_test(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, self.description)
        self.DEFAULT_FEE = 0.05
        # Setup the p2p connections and start up the network thread.
        self.test_nodes = []
        for i in range(self.num_nodes):
            self.test_nodes.append(SchillingcoinTestNode())
            self.test_nodes[i].peer_connect('127.0.0.1', p2p_port(i))

        network_thread_start()  # Start up network handling in another thread

        # Let the test nodes get in sync
        for i in range(self.num_nodes):
            self.test_nodes[i].wait_for_verack()

    def setColdStakingEnforcement(self, fEnable=True):
        # SPORK_17_COLDSTAKING_ENFORCEMENT is hard-coded enabled in SCH.
        # Keep this helper as a no-op so tests remain compatible with older test logic.
        self.log.info("Cold staking is hard-coded enabled; SPORK 17 activation skipped.")
        # small sleep to preserve test timing expectations in tests that expect a short delay
        sleep(1)

    def isColdStakingEnforced(self):
        # Always true now that SPORK_17 is removed from runtime control.
        return True

    def run_test(self):
        self.description = "Performs tests on the Cold Staking P2CS implementation"
        self.init_test()
        NUM_OF_INPUTS = 20
        INPUT_VALUE = 249

        # nodes[0] - coin-owner
        # nodes[1] - cold-staker

        # 1) nodes[0] and nodes[2] mine 25 blocks each
        # --------------------------------------------
        print("*** 1 ***")
        self.log.info("Mining 50 Blocks...")
        for peer in [0, 2]:
            for j in range(25):
                self.mocktime = self.generate_pow(peer, self.mocktime)
            sync_blocks(self.nodes)

        # 2) node[1] sends his entire balance (50 mature rewards) to node[2)
        #  - node[2] stakes a block - node[1] locks the change
        print("*** 2 ***")
        self.log.info("Emptying node1 balance")
        assert_equal(self.nodes[1].getbalance(), 50 * 250)
        txid = self.nodes[1].sendtoaddress(self.nodes[2].getnewaddress(), (50 * 250 - 0.01))
        assert (txid is not None)
        sync_mempools(self.nodes)
        self.mocktime = self.generate_pos(2, self.mocktime)
        sync_blocks(self.nodes)
        # lock the change output (so it's not used as stake input in generate_pos)
        for x in self.nodes[1].listunspent():
            assert (self.nodes[1].lockunspent(False, [{"txid": x['txid'], "vout": x['vout']}]))
        # check that it cannot stake
        sleep(1)
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], 0)

        # 3) nodes[0] generates a owner address
        #    nodes[1] generates a cold-staking address.
        # ---------------------------------------------
        print("*** 3 ***")
        owner_address = self.nodes[0].getnewaddress()
        self.log.info("Owner Address: %s" % owner_address)
        staker_address = self.nodes[1].getnewstakingaddress()
        staker_privkey = self.nodes[1].dumpprivkey(staker_address)
        self.log.info("Staking Address: %s" % staker_address)

        # 4) Check enforcement.
        # ---------------------
        print("*** 4 ***")
        # SPORK_17 has been removed; cold staking is permanently enabled in this build.
        self.log.info("Cold staking is permanently enabled in this build; skipping pre-enforcement rejection test.")

        # 5) nodes[0] delegates a number of inputs for nodes[1] to stake em.
        # ------------------------------------------------------------------
        print("*** 5 ***")
        self.log.info("First check warning when using external addresses...")
        assert_raises_rpc_error(-5, "Only the owner of the key to owneraddress will be allowed to spend these coins",
                                self.nodes[0].delegatestake, staker_address, INPUT_VALUE, "yCgCXC8N5VThhfiaVuKaNLkNnrWduzVnoT")
        self.log.info("Good. Warning triggered.")

        self.log.info("Now force the use of external address creating (but not sending) the delegation...")
        res = self.nodes[0].rawdelegatestake(staker_address, INPUT_VALUE, "yCgCXC8N5VThhfiaVuKaNLkNnrWduzVnoT", True)
        assert(res is not None and res != "")
        self.log.info("Good. Warning NOT triggered.")

        self.log.info("Now delegate with internal owner address..")
        self.log.info("Try first with a value (0.99) below the threshold")
        assert_raises_rpc_error(-8, "Invalid amount",
                                self.nodes[0].delegatestake, staker_address, 0.99, owner_address)
        self.log.info("Nice. it was not possible.")
        self.log.info("Then try (creating but not sending) with the threshold value (1.00)")
        res = self.nodes[0].rawdelegatestake(staker_address, 1.00, owner_address)
        assert(res is not None and res != "")
        self.log.info("Good. Warning NOT triggered.")

        self.log.info("Now creating %d real stake-delegation txes..." % NUM_OF_INPUTS)
        for i in range(NUM_OF_INPUTS):
            res = self.nodes[0].delegatestake(staker_address, INPUT_VALUE, owner_address)
            assert(res != None and res["txid"] != None and res["txid"] != "")
            assert_equal(res["owner_address"], owner_address)
            assert_equal(res["staker_address"], staker_address)
        sync_mempools(self.nodes)
        self.mocktime = self.generate_pos(2, self.mocktime)
        sync_blocks(self.nodes)
        self.log.info("%d Txes created." % NUM_OF_INPUTS)
        # check balances:
        self.expected_balance = NUM_OF_INPUTS * INPUT_VALUE
        self.expected_immature_balance = 0
        self.checkBalances()

        # 6) check that the owner (nodes[0]) can spend the coins.
        # -------------------------------------------------------
        print("*** 6 ***")
        self.log.info("Spending back one of the delegated UTXOs...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_equal(NUM_OF_INPUTS, len(delegated_utxos))
        assert_equal(len(delegated_utxos), len(self.nodes[0].listcoldutxos()))
        u = delegated_utxos[0]
        txhash = self.spendUTXOwithNode(u, 0)
        assert(txhash != None)
        self.log.info("Good. Owner was able to spend - tx: %s" % str(txhash))

        self.mocktime = self.generate_pos(2, self.mocktime)
        sync_blocks(self.nodes)
        # check balances after spend.
        self.expected_balance -= float(u["amount"])
        self.checkBalances()
        self.log.info("Balances check out after spend")
        assert_equal(NUM_OF_INPUTS-1, len(self.nodes[0].listcoldutxos()))

        # 7) check that the staker CANNOT use the coins to stake yet.
        # He needs to whitelist the owner first.
        # -----------------------------------------------------------
        print("*** 7 ***")
        self.log.info("Trying to generate a cold-stake block before whitelisting the owner...")
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], 0)
        self.log.info("Nice. Cold staker was NOT able to create the block yet.")

        self.log.info("Whitelisting the owner...")
        ret = self.nodes[1].delegatoradd(owner_address)
        assert(ret)
        self.log.info("Delegator address %s whitelisted" % owner_address)

        # 8) check that the staker CANNOT spend the coins.
        # ------------------------------------------------
        print("*** 8 ***")
        self.log.info("Trying to spend one of the delegated UTXOs with the cold-staking key...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_greater_than(len(delegated_utxos), 0)
        u = delegated_utxos[0]
        assert_raises_rpc_error(-26, "mandatory-script-verify-flag-failed (Script failed an OP_CHECKCOLDSTAKEVERIFY operation",
                                self.spendUTXOwithNode, u, 1)
        self.log.info("Good. Cold staker was NOT able to spend (failed OP_CHECKCOLDSTAKEVERIFY)")
        self.mocktime = self.generate_pos(2, self.mocktime)
        sync_blocks(self.nodes)

        # 9) check that the staker can use the coins to stake a block with internal miner.
        # --------------------------------------------------------------------------------
        print("*** 9 ***")
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], NUM_OF_INPUTS-1)
        self.log.info("Generating one valid cold-stake block...")
        self.mocktime = self.generate_pos(1, self.mocktime)
        self.log.info("New block created by cold-staking. Trying to submit...")
        newblockhash = self.nodes[1].getbestblockhash()
        self.log.info("Block %s submitted" % newblockhash)

        # Verify that nodes[0] accepts it
        sync_blocks(self.nodes)
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        assert_equal(newblockhash, self.nodes[0].getbestblockhash())
        self.log.info("Great. Cold-staked block was accepted!")

        # check balances after staked block.
        self.expected_balance -= INPUT_VALUE
        self.expected_immature_balance += (INPUT_VALUE + 250)
        self.checkBalances()
        self.log.info("Balances check out after staked block")

        # 10) check that the staker can use the coins to stake a block with a rawtransaction.
        # ----------------------------------------------------------------------------------
        print("*** 10 ***")
        self.log.info("Generating another valid cold-stake block...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInputs = self.get_prevouts(1, stakeable_coins)
        assert_greater_than(len(stakeInputs), 0)
        # Create the block
        new_block = self.stake_next_block(1, stakeInputs, self.mocktime, staker_privkey)
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert(ret is None)

        # Verify that nodes[0] accepts it
        sync_blocks(self.nodes)
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        assert_equal(new_block.hash, self.nodes[0].getbestblockhash())
        self.log.info("Great. Cold-staked block was accepted!")
        self.mocktime += 60
        set_node_times(self.nodes, self.mocktime)

        # check balances after staked block.
        self.expected_balance -= INPUT_VALUE
        self.expected_immature_balance += (INPUT_VALUE + 250)
        self.checkBalances()
        self.log.info("Balances check out after staked block")

        # 11) check that the staker cannot stake a block changing the coinstake scriptPubkey.
        # ----------------------------------------------------------------------------------
        print("*** 11 ***")
        self.log.info("Generating one invalid cold-stake block (changing first coinstake output)...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInputs = self.get_prevouts(1, stakeable_coins)
        assert_greater_than(len(stakeInputs), 0)
        # Create the block (with dummy key)
        new_block = self.stake_next_block(1, stakeInputs, self.mocktime, "")
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert("rejected" in ret)

        # Verify that nodes[0] rejects it
        sync_blocks(self.nodes)
        assert_raises_rpc_error(-5, "Block not found", self.nodes[0].getblock, new_block.hash)
        self.log.info("Great. Malicious cold-staked block was NOT accepted!")
        self.checkBalances()
        self.log.info("Balances check out after (non) staked block")

        # 12) neither adding different outputs to the coinstake.
        # ------------------------------------------------------
        print("*** 12 ***")
        self.log.info("Generating another invalid cold-stake block (adding coinstake output)...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInputs = self.get_prevouts(1, stakeable_coins)
        assert_greater_than(len(stakeInputs), 0)
        # Create the block
        new_block = self.stake_next_block(1, stakeInputs, self.mocktime, staker_privkey)
        # Add output (dummy key address) to coinstake (taking 100 SCH from the pot)
        self.add_output_to_coinstake(new_block, 100)
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert_equal(ret, "bad-p2cs-outs")

        # Verify that nodes[0] rejects it
        sync_blocks(self.nodes)
        assert_raises_rpc_error(-5, "Block not found", self.nodes[0].getblock, new_block.hash)
        self.log.info("Great. Malicious cold-staked block was NOT accepted!")
        self.checkBalances()
        self.log.info("Balances check out after (non) staked block")

        # 13) Now node[0] gets mad and spends all the delegated coins, voiding the P2CS contracts.
        # ----------------------------------------------------------------------------------------
        self.log.info("Let's void the contracts.")
        self.mocktime = self.generate_pos(2, self.mocktime)
        sync_blocks(self.nodes)
        print("*** 13 ***")
        self.log.info("Cancel the stake delegation spending the delegated utxos...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        # Ensure there is at least one delegated UTXO before popping the final_spend
        assert_greater_than(len(delegated_utxos), 0)
        # remove one utxo to spend later
        final_spend = delegated_utxos.pop()
        txhash = self.spendUTXOsWithNode(delegated_utxos, 0)
        assert(txhash != None)
        self.log.info("Good. Owner was able to void the stake delegations - tx: %s" % str(txhash))
        self.mocktime = self.generate_pos(2, self.mocktime)
        sync_blocks(self.nodes)

        # SPORK_17 cannot be disabled in this build; log and continue.
        self.log.info("Cold staking cannot be disabled; SPORK_17 removed. Proceeding to spend the final delegated UTXO.")
        txhash = self.spendUTXOsWithNode([final_spend], 0)
        assert(txhash != None)
        self.log.info("Good. Owner was able to void a stake delegation - tx: %s" % str(txhash))
        self.mocktime = self.generate_pos(2, self.mocktime)
        sync_blocks(self.nodes)

        # check balances after big spend.
        self.expected_balance = 0
        self.checkBalances()
        self.log.info("Balances check out after the delegations have been voided.")
        self.log.info("Cold staking remains permanently enabled.")

        # 14) check that coinstaker is empty and can no longer stake.
        # -----------------------------------------------------------
        print("*** 14 ***")
        self.log.info("Trying to generate one cold-stake block again...")
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], 0)
        self.log.info("Cigar. Cold staker was NOT able to create any more blocks.")

        # 15) check balances when mature.
        # -----------------------------------------------------------
        print("*** 15 ***")
        self.log.info("Staking 100 blocks to mature the cold stakes...")
        for i in range(2):
            for peer in [0, 2]:
                for j in range(25):
                    self.mocktime = self.generate_pos(peer, self.mocktime)
                sync_blocks(self.nodes)
        self.expected_balance = self.expected_immature_balance
        self.expected_immature_balance = 0
        self.checkBalances()
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        txhash = self.spendUTXOsWithNode(delegated_utxos, 0)
        assert (txhash != None)
        self.log.info("Good. Owner was able to spend the cold staked coins - tx: %s" % str(txhash))
        self.mocktime = self.generate_pos(2, self.mocktime)
        sync_blocks(self.nodes)
        self.expected_balance = 0
        self.checkBalances()

    def checkBalances(self):
        w_info = self.nodes[0].getwalletinfo()
        # Use .get to avoid KeyError if a field is missing; cast to float where appropriate
        delegated_balance = float(w_info.get("delegated_balance", 0.0))
        immature_delegated_balance = float(w_info.get("immature_delegated_balance", 0.0))
        cold_staking_balance = float(w_info.get("cold_staking_balance", 0.0))
        immature_cold_staking_balance = float(w_info.get("immature_cold_staking_balance", 0.0))

        self.log.info("OWNER - Delegated %f / Cold %f   [%f / %f]" % (
            delegated_balance, cold_staking_balance,
            immature_delegated_balance, immature_cold_staking_balance))
        assert_equal(delegated_balance, self.expected_balance)
        assert_equal(immature_delegated_balance, self.expected_immature_balance)
        assert_equal(cold_staking_balance, 0)

        w_info = self.nodes[1].getwalletinfo()
        delegated_balance = float(w_info.get("delegated_balance", 0.0))
        immature_delegated_balance = float(w_info.get("immature_delegated_balance", 0.0))
        cold_staking_balance = float(w_info.get("cold_staking_balance", 0.0))
        immature_cold_staking_balance = float(w_info.get("immature_cold_staking_balance", 0.0))

        self.log.info("STAKER - Delegated %f / Cold %f   [%f / %f]" % (
            delegated_balance, cold_staking_balance,
            immature_delegated_balance, immature_cold_staking_balance))
        assert_equal(delegated_balance, 0)
        assert_equal(cold_staking_balance, self.expected_balance)
        assert_equal(immature_cold_staking_balance, self.expected_immature_balance)

    def spendUTXOwithNode(self, utxo, node_n):
        if not utxo or 'txid' not in utxo or 'vout' not in utxo:
            raise ValueError("Invalid utxo provided to spendUTXOwithNode")
        new_addy = self.nodes[node_n].getnewaddress()
        inputs = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
        out_amount = (float(utxo.get("amount", 0.0)) - self.DEFAULT_FEE)
        if out_amount <= 0:
            raise ValueError("UTXO amount too small to cover fee")
        outputs = {new_addy: out_amount}
        spendingTx = self.nodes[node_n].createrawtransaction(inputs, outputs)
        spendingTx_signed = self.nodes[node_n].signrawtransaction(spendingTx)
        return self.nodes[node_n].sendrawtransaction(spendingTx_signed["hex"])

    def spendUTXOsWithNode(self, utxos, node_n):
        if not utxos:
            raise ValueError("No UTXOs provided to spendUTXOsWithNode")
        new_addy = self.nodes[node_n].getnewaddress()
        inputs = []
        total_amount = 0.0
        for utxo in utxos:
            if 'txid' not in utxo or 'vout' not in utxo:
                raise ValueError("Invalid utxo in list")
            inputs.append({"txid": utxo["txid"], "vout": utxo["vout"]})
            total_amount += float(utxo.get("amount", 0.0))
        out_amount = total_amount - self.DEFAULT_FEE
        if out_amount <= 0:
            raise ValueError("Total UTXO amount too small to cover fee")
        outputs = {new_addy: out_amount}
        spendingTx = self.nodes[node_n].createrawtransaction(inputs, outputs)
        spendingTx_signed = self.nodes[node_n].signrawtransaction(spendingTx)
        return self.nodes[node_n].sendrawtransaction(spendingTx_signed["hex"])

    def add_output_to_coinstake(self, block, value, peer=1):
        # Append an extra output to the coinstake transaction (used to test invalid coinstake outputs)
        coinstake = block.vtx[1]
        if not hasattr(self, 'DUMMY_KEY'):
            self.init_dummy_key()
        # Append the dummy output and deduct value from the coinstake reward output
        coinstake.vout.append(
            CTxOut(value * COIN, CScript([self.DUMMY_KEY.get_pubkey(), OP_CHECKSIG])))
        # Ensure there is an output at index 1 to deduct from
        if len(coinstake.vout) <= 1:
            raise RuntimeError("Unexpected coinstake vout layout when adding output")
        coinstake.vout[1].nValue -= value * COIN

        # Rebuild and re-sign the coinstake input referencing the prevout stake
        prevout = COutPoint()
        prevout.deserialize_uniqueness(BytesIO(block.prevoutStake))
        coinstake.vin[0] = CTxIn(prevout)
        stake_tx_signed_raw_hex = self.nodes[peer].signrawtransaction(
            bytes_to_hex_str(coinstake.serialize()))['hex']
        block.vtx[1] = CTransaction()
        block.vtx[1].from_hex(stake_tx_signed_raw_hex)

        # Recompute merkle root and re-sign the block
        block.hashMerkleRoot = block.calc_merkle_root()
        block.rehash()
        block.re_sign_block()

if __name__ == '__main__':
    SchillingCoin_ColdStakingTest().main()