# Copyright (c) 2013-2016 The Bitcoin Core developers
# Copyright (c) 2026 The SchillingCoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

DIST_SUBDIRS = secp256k1 univalue

AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS) $(GPROF_LDFLAGS) $(SANITIZER_LDFLAGS)

# Add the Qt compatibility flag here
AM_CXXFLAGS = $(DEBUG_CXXFLAGS) $(HARDENED_CXXFLAGS) $(WARN_CXXFLAGS) $(NOWARN_CXXFLAGS) \
  $(ERROR_CXXFLAGS) $(GPROF_CXXFLAGS) $(SANITIZER_CXXFLAGS) -Wno-deprecated-copy

AM_CPPFLAGS = $(DEBUG_CPPFLAGS) $(HARDENED_CPPFLAGS)
AM_LIBTOOLFLAGS = --preserve-dup-deps
EXTRA_LIBRARIES =

BITCOIN_CONFIG_INCLUDES = -I$(builddir)/config
BITCOIN_INCLUDES = -I$(builddir) -I$(builddir)/obj $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) \
  $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

BITCOIN_INCLUDES += -I$(srcdir)/secp256k1/include
BITCOIN_INCLUDES += $(UNIVALUE_CFLAGS)
BITCOIN_INCLUDES += -isystem$(srcdir)/dashbls/include -isystem$(srcdir)/dashbls/depends/relic/include -isystem$(srcdir)/dashbls/depends/minialloc/include
BITCOIN_INCLUDES += -isystem$(srcdir)/immer

LIBBITCOIN_NODE = libbitcoin_node.a
LIBBITCOIN_COMMON = libbitcoin_common.a
LIBBITCOIN_CONSENSUS = libbitcoin_consensus.a
LIBBITCOIN_CLI = libbitcoin_cli.a
LIBBITCOIN_UTIL = libbitcoin_util.a
LIBBITCOIN_CRYPTO = crypto/libbitcoin_crypto.a
LIBBITCOIN_BIGNUM = libbignum.a
LIBBITCOINQT = qt/libbitcoinqt.a
LIBDASHBLS = dashbls/libdashbls.la
LIBSECP256K1 = secp256k1/libsecp256k1.la

if ENABLE_ZMQ
LIBBITCOIN_ZMQ = libbitcoin_zmq.a
endif
if BUILD_BITCOIN_LIBS
LIBBITCOINCONSENSUS = libbitcoinconsensus.la
endif
if ENABLE_WALLET
LIBBITCOIN_WALLET = libbitcoin_wallet.a
endif

$(LIBDASHBLS):
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D)

$(LIBSECP256K1): $(wildcard secp256k1/src/*.h) $(wildcard secp256k1/src/*.c) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_BIGNUM) \
  $(LIBBITCOIN_NODE) \
  $(LIBBITCOIN_CLI) \
  $(LIBBITCOIN_WALLET) \
  $(LIBBITCOIN_ZMQ)

lib_LTLIBRARIES = $(LIBBITCOINCONSENSUS)
noinst_LTLIBRARIES =

bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_BITCOIND
  bin_PROGRAMS += schillingcoind
endif

if BUILD_BITCOIN_UTILS
  bin_PROGRAMS += schillingcoin-cli schillingcoin-tx
endif

.PHONY: FORCE check-symbols check-security
# schillingcoin core #
BITCOIN_CORE_H = \
  addressbook.h \
  addrdb.h \
  addrman.h \
  allocators.h \
  arith_uint256.h \
  attributes.h \
  base58.h \
  blob_uint256.h \
  bip38.h \
  bip324.h \
  bloom.h \
  blockassembler.h \
  blocksignature.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  cachemap.h \
  cachemultimap.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coinjoin/options.h \
  coins.h \
  common/args.h \
  common/bloom.h \
  common/run_command.h \
  common/settings.h \
  compat.h \
  compat/byteswap.h \
  compat/compat.h \
  compat/cpuid.h \
  compat/endian.h \
  compat/sanity.h \
  compressor.h \
  consensus/amount.h \
  consensus/consensus.h \
  consensus/merkle.h \
  consensus/params.h \
  consensus/tx_check.h \
  consensus/tx_verify.h \
  consensus/validation.h \
  context.h \
  core_io.h \
  core_memusage.h \
  cuckoocache.h \
  crypter.h \
  dbwrapper.h \
  evo/specialtx.h \
  external_signer.h \
  forgeman.h \
  governance/common.h \
  governance/governance.h \
  governance/object.h \
  governance/vote.h \
  governance/votedb.h \
  hash.h \
  httprpc.h \
  httpserver.h \
  i2p.h \
  init.h \
  interfaces/chain.h \
  interfaces/coinjoin.h \
  interfaces/echo.h \
  interfaces/handler.h \
  interfaces/node.h \
  interfaces/providertx.h \
  interfaces/wallet.h \
  kernel/blockmanager_opts.h \
  kernel/chainstatemanager_opts.h \
  kernel/checks.h \
  kernel/coinstats.h \
  kernel/context.h \
  kernel/cs_main.h \
  kernel/disconnected_transactions.h \
  kernel/mempool_persist.h \
  kernel.h \
  key.h \
  key_io.h \
  keystore.h \
  legacy/stakemodifier.h \
  limitedmap.h \
  logging/categories.h \
  logging.h \
  logging/timer.h \
  main.h \
  mapport.h \
  memusage.h \
  masternode-payments.h \
  merkleblock.h \
  messagesigner.h \
  mruset.h \
  net.h \
  netaddress.h\
  netbase.h \
  netgroup.h \
  node/blockstorage.h \
  node/chainstate.h \
  node/chainstatemanager_args.h \
  node/coin.h \
  node/context.h \
  node/miner.h \
  node/transaction.h \
  node/utxo_snapshot.h \
  noui.h \
  optional.h \
  operationresult.h \
  policy/feerate.h \
  policy/fees.h \
  policy/packages.h \
  policy/policy.h \
  pow.h \
  prevector.h \
  primitives/block.h \
  primitives/transaction.h \
  protocol.h \
  psbt.h \
  pubkey.h \
  random.h \
  randomenv.h \
  reverselock.h \
  reverse_iterate.h \
  rpc/client.h \
  rpc/protocol.h \
  rpc/request.h \
  rpc/server.h \
  rpc/server_util.h \
  scheduler.h \
  script/script_error.h \
  script/interpreter.h \
  script/keyorigin.h \
  script/script.h \
  script/sigcache.h \
  script/sign.h \
  script/signingprovider.h \
  script/standard.h \
  serialize.h \
  span.h \
  stakeinput.h \
  streams.h \
  stubs/legacy_zerocoin_consensus.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/lockedpool.h \
  sync.h \
  threadsafety.h \
  timedata.h \
  tinyformat.h \
  torcontrol.h \
  txdb.h \
  txmempool.h \
  util/threadinterrupt.h \
  guiinterface.h \
  guiinterfaceutil.h \
  uint256.h \
  undo.h \
  util.h \
  utilmoneystr.h \
  utilstrencodings.h \
  utiltime.h \
  util/asmap.h \
  util/edge.h \
  util/fastrange.h \
  util/fees.h \
  util/fs.h \
  util/macros.h \
  util/memory.h \
  util/overflow.h \
  util/readwritefile.h \
  util/settings.h \
  util/sock.h \
  util/stdmutex.h \
  util/string.h \
  util/subprocess.hpp \
  util/system.h \
  util/threadnames.h \
  util/chaintype.h \
  util/types.h \
  util/wpipe.h \
  validation.h \
  validationinterface.h \
  version.h \
  warnings.h \
  wallet/bdb.h \
  wallet/bip39.h \
  wallet/bip39_english.h \
  wallet/coincontrol.h \
  wallet/context.h \
  wallet/db.h \
  wallet/hdchain.h \
  wallet/fees.h \
  wallet/init.h \
  wallet/scriptpubkeyman.h \
  wallet/wallet.h \
  wallet/walletdb.h \
  wallet/walletutil.h \
  wallet/wallet_ismine.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h \
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
libbitcoin_util_a-clientversion.$(OBJEXT): obj/build.h

# node: shared between schillingcoind and schillingcoin-qt
libbitcoin_node_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
libbitcoin_node_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbitcoin_node_a_SOURCES = \
  active/context.cpp \
  active/dkgsession.cpp \
  active/dkgsessionhandler.cpp \
  active/masternode.cpp \
  addrdb.cpp \
  addrman.cpp \
  banman.cpp \
  batchedlogger.cpp \
  bip324.cpp \
  blockassembler.cpp \
  blockencodings.cpp \
  blockfilter.cpp \
  blocksignature.cpp \
  chain.cpp \
  chainlock/chainlock.cpp \
  chainlock/clsig.cpp \
  chainlock/handler.cpp \
  chainlock/signing.cpp \
  checkpoints.cpp \
  consensus/tx_verify.cpp \
  dbwrapper.cpp \
  deploymentstatus.cpp \
  dsnotificationinterface.cpp \
  evo/cbtx.cpp \
  evo/chainhelper.cpp \
  evo/deterministicmns.cpp \
  evo/dmnstate.cpp \
  evo/evodb.cpp \
  evo/mnauth.cpp \
  evo/mnhftx.cpp \
  evo/providertx.cpp \
  evo/providertx_service.cpp \
  evo/simplifiedmns.cpp \
  evo/smldiff.cpp \
  evo/snapshot.cpp \
  evo/specialtx.cpp \
  evo/specialtx_filter.cpp \
  evo/specialtxman.cpp \
  flatfile.cpp \
  gsl/assert.cpp \
  httprpc.cpp \
  httpserver.cpp \
  i2p.cpp \
  index/addressindex.cpp \
  index/addressindex_util.cpp \
  index/base.cpp \
  index/blockfilterindex.cpp \
  index/coinstatsindex.cpp \
  index/spentindex.cpp \
  index/timestampindex.cpp \
  index/txindex.cpp \
  init.cpp \
  instantsend/db.cpp \
  instantsend/instantsend.cpp \
  instantsend/lock.cpp \
  instantsend/net_instantsend.cpp \
  instantsend/signing.cpp \
  kernel/chain.cpp \
  kernel/checks.cpp \
  kernel/coinstats.cpp \
  kernel/context.cpp \
  kernel/cs_main.cpp \
  kernel/mempool_persist.cpp \
  llmq/blockprocessor.cpp \
  llmq/commitment.cpp \
  llmq/context.cpp \
  llmq/debug.cpp \
  llmq/dkgsession.cpp \
  llmq/dkgsessionhandler.cpp \
  llmq/dkgsessionmgr.cpp \
  llmq/ehf_signals.cpp \
  llmq/net_dkg.cpp \
  llmq/net_quorum.cpp \
  llmq/net_signing.cpp \
  llmq/observer.cpp \
  llmq/options.cpp \
  llmq/quorumproofs.cpp \
  llmq/quorums.cpp \
  llmq/quorumsman.cpp \
  llmq/signhash.cpp \
  llmq/signing.cpp \
  llmq/signing_shares.cpp \
  llmq/snapshot.cpp \
  llmq/utils.cpp \
  mapport.cpp \
  masternode/meta.cpp \
  masternode/payments.cpp \
  masternode/sync.cpp \
  net.cpp \
  net_processing.cpp \
  netfulfilledman.cpp \
  netgroup.cpp \
  node/blockstorage.cpp \
  node/caches.cpp \
  node/chainstate.cpp \
  node/chainstatemanager_args.cpp \
  node/coin.cpp \
  node/connection_types.cpp \
  node/context.cpp \
  node/eviction.cpp \
  node/interface_ui.cpp \
  node/interfaces.cpp \
  node/mempool_args.cpp \
  node/mempool_persist_args.cpp \
  node/miner.cpp \
  node/transaction.cpp \
  node/txreconciliation.cpp \
  node/utxo_snapshot.cpp \
  node/validation_cache_args.cpp \
  noui.cpp \
  policy/fees.cpp \
  policy/fees_args.cpp \
  policy/packages.cpp \
  policy/policy.cpp \
  policy/settings.cpp \
  pow.cpp \
  rest.cpp \
  rpc/blockchain.cpp \
  rpc/masternode.cpp \
  rpc/mining.cpp \
  rpc/misc.cpp \
  rpc/net.cpp \
  rpc/rawtransaction.cpp \
  rpc/server.cpp \
  rpc/server_util.cpp \
  script/sigcache.cpp \
  shutdown.cpp \
  stats/client.cpp \
  stats/rawsender.cpp \
  timedata.cpp \
  torcontrol.cpp \
  txdb.cpp \
  txmempool.cpp \
  txorphanage.cpp \
  txrequest.cpp \
  validation.cpp \
  validationinterface.cpp \
  versionbits.cpp \
  $(BITCOIN_CORE_H)

if ENABLE_ZMQ
libbitcoin_zmq_a_CPPFLAGS = $(BITCOIN_INCLUDES) $(ZMQ_CFLAGS)
libbitcoin_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbitcoin_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif

# wallet: shared between schillingcoind and schillingcoin-qt, but only linked
# when wallet enabled
libbitcoin_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libbitcoin_wallet_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbitcoin_wallet_a_SOURCES = \
  bip38.cpp \
  addressbook.cpp \
  wallet/crypter.cpp \
  masternode-payments.cpp \
  forgeman.cpp \
  messagesigner.cpp \
  legacy/stakemodifier.cpp \
  kernel.cpp \
  wallet/bdb.cpp \
  wallet/coincontrol.cpp \
  wallet/context.cpp \
  wallet/db.cpp \
  wallet/fees.cpp \
  wallet/init.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/hdchain.cpp \
  wallet/scriptpubkeyman.cpp \
  wallet/wallet.cpp \
  wallet/wallet_ismine.cpp \
  wallet/walletdb.cpp \
  wallet/walletutil.cpp \
  stakeinput.cpp \
  $(BITCOIN_CORE_H)

# crypto primitives library
crypto_libbitcoin_crypto_a_CPPFLAGS = $(AM_CPPFLAGS) $(PIC_FLAGS)
crypto_libbitcoin_crypto_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIC_FLAGS)
crypto_libbitcoin_crypto_a_SOURCES = \
  crypto/aes.cpp \
  crypto/aes.h \
  crypto/sha1.cpp \
  crypto/sha3.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  crypto/siphash.cpp \
  crypto/siphash.h \
  crypto/chacha20.cpp \
  crypto/chacha20.h \
  crypto/chacha20poly1305.cpp \
  crypto/chacha20poly1305.h \
  crypto/hex_base.cpp \
  crypto/hex_base.h \
  crypto/hkdf_sha256_32.cpp \
  crypto/hkdf_sha256_32.h \
  crypto/hmac_sha256.cpp \
  crypto/rfc6979_hmac_sha256.cpp \
  crypto/hmac_sha512.cpp \
  crypto/muhash.cpp \
  crypto/muhash.h \
  crypto/poly1305.cpp \
  crypto/poly1305.h \
  crypto/scrypt.cpp \
  crypto/ripemd160.cpp \
  crypto/sph_md_helper.c \
  crypto/sph_sha2big.c \
  crypto/aes_helper.c \
  crypto/blake.c \
  crypto/bmw.c \
  crypto/groestl.c \
  crypto/jh.c \
  crypto/luffa.c \
  crypto/keccak.c \
  crypto/skein.c \
  crypto/cubehash.c \
  crypto/shavite.c \
  crypto/shabal.c \
  crypto/simd.c \
  crypto/echo.c \
  crypto/whirlpool.c \
  crypto/haval.c \
  crypto/haval_helper.c \
  crypto/hamsi.c \
  crypto/hamsi_helper.c \
  crypto/fugue.c \
  crypto/sha2.c \
  crypto/common.h \
  crypto/sha256.h \
  crypto/sha512.h \
  crypto/hmac_sha256.h \
  crypto/rfc6979_hmac_sha256.h \
  crypto/hmac_sha512.h \
  crypto/scrypt.h \
  crypto/sha1.h \
  crypto/ripemd160.h \
  crypto/sph_blake.h \
  crypto/sph_bmw.h \
  crypto/sph_groestl.h \
  crypto/sph_jh.h \
  crypto/sph_keccak.h \
  crypto/sph_luffa.h \
  crypto/sph_shavite.h \
  crypto/sph_hamsi.h \
  crypto/sph_cubehash.h \
  crypto/sph_skein.h \
  crypto/sph_simd.h \
  crypto/sph_fugue.h \
  crypto/sph_echo.h \
  crypto/sph_shabal.h \
  crypto/sph_whirlpool.h \
  crypto/sph_sha2.h \
  crypto/sph_haval.h \
  crypto/sph_types.h

# bignum library
libbignum_a_CPPFLAGS = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS)
libbignum_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbignum_a_SOURCES = \
  bignum/bignum.h \
  bignum/bignum.cpp

if USE_NUM_GMP
  libbignum_a_SOURCES += bignum/bignum_gmp.cpp
endif

if USE_NUM_OPENSSL
  libbignum_a_SOURCES += bignum/bignum_openssl.cpp
endif

# common: shared between schillingcoind, schillingcoin-qt and non-server tools
libbitcoin_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libbitcoin_common_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbitcoin_common_a_SOURCES = \
  allocators.cpp \
  base58.cpp \
  wallet/bip39.cpp \
  bip38.cpp \
  chainparams.cpp \
  coins.cpp \
  common/args.cpp \
  common/bloom.cpp \
  common/interfaces.cpp \
  common/run_command.cpp \
  common/settings.cpp \
  common/url.cpp \
  compressor.cpp \
  consensus/merkle.cpp \
  core_read.cpp \
  core_write.cpp \
  evo/core_write.cpp \
  evo/netinfo.cpp \
  evo/providertx_util.cpp \
  evo/specialtx.cpp \
  external_signer.cpp \
  hash.cpp \
  wallet/hdchain.cpp \
  kernel/chainparams.cpp \
  key.cpp \
  key_io.cpp \
  keystore.cpp \
  llmq/core_write.cpp \
  merkleblock.cpp \
  net_permissions.cpp \
  net_types.cpp \
  netaddress.cpp \
  netbase.cpp \
  netgroup.cpp \
  outputtype.cpp \
  policy/feerate.cpp \
  primitives/block.cpp \
  primitives/transaction.cpp \
  protocol.cpp \
  psbt.cpp \
  pubkey.cpp \
  rpc/rawtransaction_util.cpp \
  rpc/request.cpp \
  rpc/util.cpp \
  saltedhasher.cpp \
  scheduler.cpp \
  script/descriptor.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  script/sign.cpp \
  script/signingprovider.cpp \
  script/standard.cpp \
  script/script_error.cpp \
  warnings.cpp \
  $(BITCOIN_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libbitcoin_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libbitcoin_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbitcoin_util_a_SOURCES = \
  allocators.cpp \
  arith_uint256.cpp \
  blob_uint256.cpp \
  bls/bls_ies.cpp \
  bls/bls_worker.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  logging.cpp \
  random.cpp \
  randomenv.cpp \
  rpc/protocol.cpp \
  support/cleanse.cpp \
  support/lockedpool.cpp \
  streams.cpp \
  sync.cpp \
  uint256.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  util/asmap.cpp \
  util/bip32.cpp \
  util/bytevectorhash.cpp \
  util/chaintype.cpp \
  util/check.cpp \
  util/edge.cpp \
  util/error.cpp \
  util/fees.cpp \
  util/fs.cpp \
  util/fs_helpers.cpp \
  util/getuniquepath.cpp \
  util/hasher.cpp \
  util/message.cpp \
  util/moneystr.cpp \
  util/ranges_set.cpp \
  util/readwritefile.cpp \
  util/settings.cpp \
  util/sock.cpp \
  util/spanparsing.cpp \
  util/string.cpp \
  util/syserror.cpp \
  util/system.cpp \
  util/thread.cpp \
  util/threadinterrupt.cpp \
  util/threadnames.cpp \
  util/time.cpp \
  util/tokenpipe.cpp \
  util/wpipe.cpp \
  $(BITCOIN_CORE_H)

if GLIBC_BACK_COMPAT
libbitcoin_util_a_SOURCES += compat/glibc_compat.cpp
AM_LDFLAGS += $(COMPAT_LDFLAGS)
endif

#
# consensus #
libbitcoin_consensus_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES) $(BOOST_CPPFLAGS)
libbitcoin_consensus_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbitcoin_consensus_a_SOURCES = \
  arith_uint256.cpp \
  arith_uint256.h \
  bls/bls.cpp \
  bls/bls.h \
  consensus/amount.h \
  consensus/merkle.cpp \
  consensus/merkle.h \
  consensus/params.h \
  consensus/tx_check.cpp \
  consensus/tx_check.h \
  consensus/validation.h \
  hash.cpp \
  hash.h \
  prevector.h \
  primitives/block.cpp \
  primitives/block.h \
  primitives/transaction.cpp \
  primitives/transaction.h \
  pubkey.cpp \
  pubkey.h \
  script/bitcoinconsensus.cpp \
  script/interpreter.cpp \
  script/interpreter.h \
  script/script.cpp \
  script/script.h \
  script/script_error.cpp \
  script/script_error.h \
  serialize.h \
  span.h \
  streams.h \
  tinyformat.h \
  uint256.cpp \
  uint256.h \
  util/strencodings.cpp \
  util/strencodings.h \
  util/string.cpp \
  util/string.h \
  version.h
#

# cli: shared between schillingcoin-cli and schillingcoin-qt
libbitcoin_cli_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libbitcoin_cli_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbitcoin_cli_a_SOURCES = \
  rpc/client.cpp \
  $(BITCOIN_CORE_H)

nodist_libbitcoin_util_a_SOURCES = $(srcdir)/obj/build.h
#

# schillingcoind binary #
schillingcoind_SOURCES = schillingcoind.cpp
schillingcoind_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
schillingcoind_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
schillingcoind_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
schillingcoind_SOURCES += schillingcoind-res.rc
endif
schillingcoind_LDADD = \
  $(LIBBITCOIN_NODE) \
  $(LIBBITCOIN_WALLET) \
  $(LIBBITCOIN_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_BIGNUM) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_ZMQ) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBDASHBLS) \
  $(LIBLEVELDB) \
  $(LIBLEVELDB_SSE42) \
  $(LIBMEMENV) \
  $(LIBSECP256K1)

schillingcoind_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(ZMQ_LIBS)

# schillingcoin-cli binary #
schillingcoin_cli_SOURCES = schillingcoin-cli.cpp
schillingcoin_cli_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES) $(EVENT_CFLAGS)
schillingcoin_cli_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
schillingcoin_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
schillingcoin_cli_SOURCES += schillingcoin-cli-res.rc
endif

schillingcoin_cli_LDADD = \
  $(LIBBITCOIN_CLI) \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CRYPTO)

schillingcoin_cli_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(EVENT_LIBS)
#

# schillingcoin-tx binary #
schillingcoin_tx_SOURCES = schillingcoin-tx.cpp
schillingcoin_tx_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
schillingcoin_tx_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
schillingcoin_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
schillingcoin_tx_SOURCES += schillingcoin-tx-res.rc
endif

schillingcoin_tx_LDADD = \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_BIGNUM) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBSECP256K1)

schillingcoin_tx_LDADD += $(BOOST_LIBS) $(CRYPTO_LIBS)
#

# bitcoinconsensus library #
if BUILD_BITCOIN_LIBS
include_HEADERS = script/bitcoinconsensus.h
libbitcoinconsensus_la_SOURCES = \
  allocators.cpp \
  arith_uint256.cpp \
  blob_uint256.cpp \
  primitives/transaction.cpp \
  crypto/hmac_sha512.cpp \
  crypto/scrypt.cpp \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  crypto/ripemd160.cpp \
  hash.cpp \
  pubkey.cpp \
  script/script.cpp \
  script/interpreter.cpp \
  script/bitcoinconsensus.cpp \
  uint256.cpp \
  utilstrencodings.cpp

if GLIBC_BACK_COMPAT
  libbitcoinconsensus_la_SOURCES += compat/glibc_compat.cpp
endif

libbitcoinconsensus_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined $(RELDFLAGS)
libbitcoinconsensus_la_LIBADD = $(LIBSECP256K1)
libbitcoinconsensus_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_BITCOIN_INTERNAL
libbitcoinconsensus_la_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)

endif
#

CLEANFILES = $(EXTRA_LIBRARIES)

CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += consensus/*.gcda consensus/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += bignum/*.gcda bignum/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += rpc/*.gcda rpc/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += support/*.gcda support/*.gcno
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += wallet/*.gcda wallet/*.gcno
CLEANFILES += wallet/test/*.gcda wallet/test/*.gcno
CLEANFILES += zmq/*.gcda zmq/*.gcno
CLEANFILES += zsch/*.gcda zsch/*.gcno
CLEANFILES += obj/build.h

CTAES_DIST =  crypto/ctaes/bench.c
CTAES_DIST += crypto/ctaes/ctaes.c
CTAES_DIST += crypto/ctaes/ctaes.h
CTAES_DIST += crypto/ctaes/README.md
CTAES_DIST += crypto/ctaes/test.c

EXTRA_DIST = $(CTAES_DIST)


config/schillingcoin-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/schillingcoin-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/schillingcoin-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/schillingcoin-config.h.in

clean-local:
	-$(MAKE) -C secp256k1 clean
	-$(MAKE) -C univalue clean
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h
	-rm -rf test/__pycache__

.rc.o:
	@test -f $(WINDRES)
	## FIXME: How to get the appropriate modulename_CPPFLAGS in here?
	$(AM_V_GEN) $(WINDRES) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) -DWINDRES_PREPROC -i $< -o $@

check-symbols: $(bin_PROGRAMS)
if GLIBC_BACK_COMPAT
	@echo "Checking glibc back compat..."
	$(AM_V_at) READELF=$(READELF) CPPFILT=$(CPPFILT) $(PYTHON) $(top_srcdir)/contrib/devtools/symbol-check.py < $(bin_PROGRAMS)
endif

check-security: $(bin_PROGRAMS)
if HARDEN
	@echo "Checking binary security..."
	$(AM_V_at) READELF=$(READELF) OBJDUMP=$(OBJDUMP) $(PYTHON) $(top_srcdir)/contrib/devtools/security-check.py < $(bin_PROGRAMS)
endif

if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif

include Makefile.univalue.include