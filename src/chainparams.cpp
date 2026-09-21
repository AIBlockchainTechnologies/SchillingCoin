// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"
#include "random.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>
#include <limits>

#include "chainparamsseeds.h"

std::string CDNSSeedData::getHost(uint64_t requiredServiceBits) const {
    // Use default host for non-filter-capable seeds or if we use the default service bits (NODE_NETWORK)
    if (!supportsServiceBitsFiltering || requiredServiceBits == NODE_NETWORK)
        return host;

    return strprintf("x%x.%s", requiredServiceBits, host);
}

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock.SetNull();
    genesis.nVersion = nVersion;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of the genesis coinbase cannot
 * be spent as it did not originally exist in the database.
 *
 * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
 *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
 *   vMerkleTree: e0028e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "Schilling geht mit ansehnlicher Bilanz";
    const CScript genesisOutputScript = CScript() << ParseHex("042292b1f401860eea99e1a8a103effbd7e1c013a59a1a3a0c91c9d1997a0bc6f338567278c11344802838c107055bf7c1641eaed61e879245c255a4f5be5746fc") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main Schilling Coin (SCH) Network
 */

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress>& vSeedsOut, const SeedSpec6* data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    for (unsigned int i = 0; i < count; i++) {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

//   What makes a good checkpoint block?
// + Is surrounded by blocks with reasonable timestamps
//   (no blocks before with a timestamp after, none after with
//   timestamp before).
// + Contains no strange transactions, at all.
static Checkpoints::MapCheckpoints mapCheckpoints =
    boost::assign::map_list_of
	(0, uint256("0x00000f18d3e3a5bf9abee0cb36483706e8392a3f1b0f23d6df5a8fdc30303e74"))
	(2140, uint256("0xa52d5be17db090d372ca8e179bd241d025e326839e73da7292dcdf19e8eeb3dd"))
	(9385, uint256("0x0f790b79a8a5e7e217b6ce9506b5024bdbc279e2984196a0d25e907131da2b53"))
	(35420, uint256("0x664775a61764f7ead3c66348b3c3d7c649d5c315b5a626d34779046983a3b109"))
	(47687, uint256("0x9a9a8fbb169ee41ce5fe8d40f6f2431c4d067822a722f175d9c4c2c12d8f4c11"))
	(62431, uint256("0x31e5e71e6e310cc41df8010dd8973d932aed8d0d0a5309ab5a2d8a1dba474fc4"))
	(92567, uint256("0xbe57a7152faaec04ebc380a0f6d2e48f8ff9898fd36b212a4021a28f23199646"))
	(120955, uint256("0x2ef7f5d2ca893430bbd13f056e4752c3da79b28ea55caf98b8460feb27bf7d84"))
	(145826, uint256("0x72d644bdbb0ea1eb40865e88261acacb3e4bfe0699ea6dc4830b727b34c9ac42"))
	(187547, uint256("0xeff4b637ea8c38f32e9d7347c66a5232a4d9f067dc8300b8c2706f75a0c3c17e"))
	(205041, uint256("0xd3dea55726c3f0067d7aaeb59863ba983cc704b8dc973e3eaffe6a2c06b235e7"))
	(239125, uint256("0x3baa75b73b214d8ace4531804f4666d6c08d87e424191183fef9e674d8ed98b4"))
	(270295, uint256("0xeb54601c3c6d59b74d51e8ebdfd6651fcb921de8a24a0b92990851a53d936b79"))
	(291849, uint256("0x7646c81136c5ff63192bbe07f722287f3f0369421f6108d0798b2ff2b460fa28"))
	(304055, uint256("0xd741af157776a43992a9fb97d055dfadc6434efa0650675500d04d42c6641d8d"))
	(468254, uint256("0x39fee6b0f0e61192e418a60e04660c5b57ea3bcc7165ac58d7a3cbbbf7e68266"))
	(571249, uint256("0x97bca2697e838dc021ba5faa0ec27875bd7dffc6d875dc06d4f173d9b157dfaa"))
	(661279, uint256("0xb4cc96ec85f2813fa888d11da4f0977ff680d9ef5a3b0be1230c1e7e187c47c8"))
	(732687, uint256("0xb67b941c3962e26985726df80b0270b6be74b11660e2ca29581f1737d3be8df4"))
	(872345, uint256("0x89f248d91b048183e187ba19761ae604d84b289d3e0bc6559934a4495fdb22b2"))
	(999407, uint256("0x0ba62e526240bdfbd4cbe8853305d53d8336f64bf64b48ac185de07d34ca10b2"))
	(1042607, uint256("0xd8ed71f0ee7c008c648015fca80e74ed0a1308d4a9962bc039c93a4f66b0247f"))
	(1085807, uint256("0xd6f0375f1b2c7d6873b2d4cab045d5fa468f96a0ade720f8c3e1d19f37f0f24f"))
	(1129007, uint256("0x6456d43d7e6650963896f0aae0f4eb8f92bf80dbacb873bb575da7ce99102f34"))
	(1172207, uint256("0x64994dabbcbea7e37cef4f3d75c2cf9e482c904fd3ffb4b8885ce21e222af5e6"))
	(1215407, uint256("0x38ae392189c6fd6575be72a7bb738eb049304688101ced89471741f92d002ac8"))
	(1258607, uint256("0x1ad5e923e21934028bdfd4ec0fd7618e87b9ab49173abd9a6ceb3eb08f12b642"))
	(1301807, uint256("0xfbfeac75a7ba2f606c5d2f2e62aaa8edc244983bfc3ae023d45b0b353d33ec81"))
	(1345007, uint256("0xb3893c74ed783361ca0f47769ada59bcfff93d9686a21c098f6e58c37d7e1247"))
	(1388207, uint256("0x6f69ca122ece413be21c320a8ac98278ea5193cab3d68a0ce7efb7a955017f9d"))
	(1431407, uint256("0x5ee908141f830b12821ead5f7d5a0b5e1b677fc5de926a82c38d08ac31e7f234"))
	(1474607, uint256("0xee87f87736a1f37993f00f9629892fd54fc1488d5241925c470d34ae50a44a27"))
	(1517807, uint256("0xdb951239cb6333545a8b7aedf0dfacdbc22bfcc461c32b5e0d9f128101f16d3c"))
	(1561007, uint256("0xab4d49c74219eec63090f2a821151e6c885b186e35534133c07d71c7fc43c600"))
	(1604207, uint256("0xbf22e34785a7eb65ae1ca2f5a289b6a79c9cae1f66b39dedb2ec851c9f7f76fa"))
	(1647407, uint256("0x6d3fb49310b9b88d844093c69e9191f6957f205a9ae1a873b0a91ea2640c8fcc"))
	(1690607, uint256("0xd7b27001a80411dadcf53444feffeb8b9af8f0fa854121fd0bdf78393f6664ad"))
	(1733807, uint256("0x6b5d911dd39ec2a326f8c934487c88874a38ba5fb870a9506349f55eddd1712c"))
	(1777007, uint256("0xffaa6d7eecab95c3d1fdaaf9e79cb5efd71a7488a1022688649ea3f28fb54e4a"))
	(1820207, uint256("0x0afb553a93f2f53c31fdcfb485eb671eb76124aeb43ac3156824ab43ba2c66e6"))
	(1863407, uint256("0xae66164b4fbd1c6d657793f19474afd0758bbac10c327c4ca87d6c5e85babe83"))
	(1906607, uint256("0xe2db1bb88f26fa537fdca3e8dd1ce6be98a06404b533402b7f9de0d63a87d60d"))
	(1949807, uint256("0xe99afedae95f4538a25fb2adf654a1d1f4e6ffd3cf50eda6a66c10ab47332a62"))
	(1993007, uint256("0x90769a5d0b8da938b3094c5cb460ccf7077c79a903f033a695d4911a46f27701"))
	(2036207, uint256("0x1197806e4d47891016b9a1188542b72a8a0ebacafb32d07db1a91e78177f4d18"))
	(2079407, uint256("0x5570edff29bbc3b43bc2d1ea0675f3945dc1a8b83181cca35fe8b43285405f15"))
	(2122607, uint256("0x4467a7584c6938a4f4bebffa5680317f7057bd1da6af0c88b220779df307a36a"))
	(2165807, uint256("0x7b41c2ae61efc57b3897d8ec7fb6e945ac1e10502200a9c82959dece335bf3bc"))
	(2209007, uint256("0x1d1eb1c366ed5c1ecbdc2c642a601716431d5acfdd3bc463e5d92390d2a57593"))
	(2252207, uint256("0x37d6c24e56b05b0fbf6a97695ed4c6937fa107973cb6f10d0220f915817194c5"))
	(2295407, uint256("0x0c7f77711a1ff598b7ba9dd9cc7e213ad30f9707e6d59e45b5f7ee2d85125fe0"))
	(2338607, uint256("0xe283f7a558e672ebba0cacbe72ea9b4e0811b3f9d8a71c8fa52f93b723d92dfa"))
	(2381807, uint256("0x71c56c0af5ede04c6aa61a32ff6be89ef2088f18b05a7a6b44fed5e77052fd76"))
	(2425007, uint256("0x8fd0a9a90d29a61aa40d0359cc10f1dcdbde79cfbe92dfb5e701b997cb15eab3"))
	(2468207, uint256("0xdcd0947f1ba40ca5489411910d086fdc26bd8a5d98d2e5f33a5a57406f9ba356"))
	(2511407, uint256("0x0ea3846d53c9642a7bd8ba0c6e40eda24de0f5c25428e42d0dc2d03f55d931cc"))
	(2554607, uint256("0xfd67695fe648e1b5dda63d905b1179c4a2ccf37746858d08c5ed01424f22cf8a"))
	(2597807, uint256("0x8c36d4d2fe31c89a4c74778d6b06f3464192e91c6e1416c44e062ac5d2455946"))
	(2641007, uint256("0x2036014920de191f689ef5b616f80d901c4de6d9f1285a26747779e4b5afa9e3"))
	(2684207, uint256("0x0f650b7b8600a1730d34202e755107072a45bc4790d671ff2b0bfe11ef550278"))
	(2727407, uint256("0xbf4deb612cf339a55553b99b0613594eb0f3228c9a2ae0f93baeb6c6874e54e5"))
	(2770607, uint256("0x569c9c567aec19b737cf23ee2b76cae152ab31b316be15aff8cf7e343bedd1e3"))
	(2813807, uint256("0x864cf9a8b99ab974ca9ba69184022142a40fcace53d977dd07ebc1bca9e9f131"))
	(2857007, uint256("0x70a48a925627c96a13143f20cc9b77a0f05f24e52b354fb7f34934fc51afe6b9"))
	(2900207, uint256("0xfa00ffd068feb941ab2ffb7fdffdc84fffad67e039bfec6160924d05e7c36258"))
	(2943407, uint256("0xd0f2f347ca4b0d63ee6deb6643d49a4f029d3bc7ec0aa17ce971ec9a541978f3"))
	(2986607, uint256("0x2db3fabbf3185059c4765bc8307338f0afcb776d1a6c3e2a3f9cc0d3ea6e45e5"))
	(3029807, uint256("0xeb1c24b3a5afca8a2c85b58414db26a9cc34190753d65f478019e93305dca595"))
	(3073007, uint256("0xda956f89aaa8c295ca61fe66e85dfb80a78d4afe2d68c968293a1645d8cef60d"))
	(3116207, uint256("0x8452a85a807f134f170c73c8fdd8dbcf65320863c9c2699194b39763ba47b749"))
	(3159407, uint256("0x218b07f3f157b4bd8fdb6a1060db42d98ba5a49b072726ab568a184d95630902"))
	(3202607, uint256("0xffb4d74fbd21c34ba85a7061e065a4ea6c46fc7c0273aa2e2601f97519670363"))
	(3245807, uint256("0x7d00c19bfe7dc6dd70217cd8d8ef0152abb4f3b7ffb4a103bfbaf78a9a15c6af"))
	(3289007, uint256("0x9cabc370b3e6f01c6cf6f67a4b98780d0a0fefe718bfdb2e52e1203a4f35351b"))
	(3332207, uint256("0x74e68cd1bbba37c34955b0b6e5d519f57cf7c3b9c8fc6c136ae7a1989192e37a"))
	(3375407, uint256("0x7b8678b58ac48d35f3aa2dd15f8e53787c3f571e7e69f63a994967c6e803e371"))
	(3418607, uint256("0x24ced3d82c5ff07591397c2dfb8f8c005511f8f4d81fa6fb6b8755bf4f6ddb6b"))
	(3461807, uint256("0x3b5dc49344f1c8238713e35850f6041d6ebd3ab1af7be88626d9d4e7f922791b"))
	(3505007, uint256("0x3832eba4824d31aefb9ceb6e7ed21a306b5141c78755e56725bfaf22c1c5ae3f"))
	(3548207, uint256("0x4e832873f726a5afb813ac537022d479399e9e795207f2d964a07a282a39877e"))
	(3591407, uint256("0x8013a6fd72c9a62806768da2197b8923e8633e6e2613ffc4ffc0ef3cabac0ae5"))
	(3634607, uint256("0xf0e500e14949347ab754d4ecca75e510c3511703d9506d02b4564108ce754199"))
	(3677807, uint256("0x88f70f08bc8239cc3606ae7e7b79557793622a7d30c072254dc59cb3d372710d"))
	(3721007, uint256("0xcb4789ae77c3d362df198436f3100829afb7dce475d58fc8cfa8c4cf1c588a6d"))
	;
static const Checkpoints::CCheckpointData data = {
    &mapCheckpoints
};

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
    boost::assign::map_list_of
    (0, uint256S("0x001"))
    ; //!< First v7 block
static const Checkpoints::CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    1575145155,
    2971390,
    250};

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
    boost::assign::map_list_of(0, uint256S("0x001"));
static const Checkpoints::CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    1454124731,
    0,
    100};

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";

		genesis = CreateGenesisBlock(1540198678, 1057070, 0x1e0ffff0, 1, 0 * COIN);
		consensus.hashGenesisBlock = genesis.GetHash();
		assert(consensus.hashGenesisBlock == uint256S("0x00000f18d3e3a5bf9abee0cb36483706e8392a3f1b0f23d6df5a8fdc30303e74"));
		assert(genesis.hashMerkleRoot == uint256S("0xfc3a25fe5baa1bbeb8594a751a6c78e36553262793f0017897582766fb93c2e3"));

        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.powLimit   = ~UINT256_ZERO >> 20;   // SchillingCoin starting difficulty is 1 / 2^12
        consensus.posLimitV1 = ~UINT256_ZERO >> 24;
        consensus.posLimitV2 = ~UINT256_ZERO >> 20;
        consensus.nBudgetCycleBlocks = 43200;       // approx. 1 every 30 days
        consensus.nBudgetFeeConfirmations = 6;      // Number of confirmations for the finalization fee
        consensus.nCoinbaseMaturity = 49;
        consensus.nFutureTimeDriftPoW = 7200;
        consensus.nFutureTimeDriftPoS = 180;
        consensus.nMasternodeCountDrift = 20;       // num of MN we allow the see-saw payments to be off by
        consensus.nMaxMoneyOut = 200000000 * COIN;
        consensus.nPoolMaxTransactions = 3;
        consensus.nProposalEstablishmentTime = 60 * 60 * 24;    // must be at least a day old to make it into a budget
        consensus.nStakeMinAge = 60 * 60;
        consensus.nStakeMinDepth = 60;
        consensus.nTargetTimespan = 1 * 60;
        consensus.nTargetTimespanV2 = 30 * 60;
        consensus.nTargetSpacing = 1 * 60;
        consensus.nTimeSlotLength = 15;
        consensus.strObfuscationPoolDummyAddress = "Scax8jHDQ1s2kHVjysEoTQncVdUrNBuXtp";

        // Height-Based Activations
        consensus.height_last_PoW = 100;
        consensus.height_RHF = 1040000;
        consensus.height_last_ZC_AccumCheckpoint = consensus.height_RHF;
        consensus.height_start_BIP65 = consensus.height_RHF; // 82629b7a9978f5c7ea3f70a12db92633a7d2e436711500db28b97efd48b1e527
        consensus.height_start_MessSignaturesV2 = consensus.height_RHF; // TimeProtocolV2, Blocks V7 & newMessageSignatures.
        consensus.height_start_StakeModifierNewSelection = consensus.height_RHF;
        consensus.height_start_StakeModifierV2 = consensus.height_RHF;
        consensus.height_start_TimeProtoV2 = consensus.height_RHF; // TimeProtocolV2, Blocks V7 & newMessageSignatures.
        consensus.height_start_ZC = 101;
        consensus.height_start_ZC_PublicSpends = consensus.height_RHF;
        consensus.height_start_ZC_SerialRangeCheck = consensus.height_RHF;
        consensus.height_start_ZC_SerialsV2 = consensus.height_RHF;

        // Zerocoin Related Params
        consensus.ZC_Modulus = "25416044959300777136321520272618123884878267469872720876714900694524520811086165804477622260391542"
				 "2916860912160429994534893992485623461680952402850356984626817518270156621093170424291386667786210966842060702105"
				 "2320758263069697896162353595102898408878438850598468270121556127949198934557359928793927953409628613916133280199"
				 "6828596658805689277703759074716461560351944767450363107706645282779897426524362295565115954425239545008562820285"
				 "5881818801840449384968780789221307030514472035561220067909352881149399071938126011942287899988604632460316027932"
				 "06062300642564791260379134835348071328338628080107357300760134237417397";
        consensus.ZC_MaxPublicSpendsPerTx = 7; // Assume about 220 bytes each input.
        consensus.ZC_MaxSpendsPerTx = 7; // Assume about 20kb each input.
        consensus.ZC_MinMintConfirmations = 20;
        consensus.ZC_MinMintFee = 1 * CENT;
        consensus.ZC_MinStakeDepth = 20;
        consensus.ZC_TimeStart = 1541434110; // October 17, 2017 4:30:00 AM.

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
		pchMessageStart[0] = 0x63;
		pchMessageStart[1] = 0x43;
		pchMessageStart[2] = 0x49;
		pchMessageStart[3] = 0x56;
        nDefaultPort = 9070;

        // Note that of those with the service bits flag, most only support a subset of possible options.
		vSeeds.push_back(CDNSSeedData("seed1", "s1.schillingcoin.org"));
		vSeeds.push_back(CDNSSeedData("seed2", "s2.schillingcoin.org"));
		vSeeds.push_back(CDNSSeedData("seed3", "s3.schillingcoin.org"));
		vSeeds.push_back(CDNSSeedData("seed4", "s4.schillingcoin.org"));
		vSeeds.push_back(CDNSSeedData("seed5", "s5.schillingcoin.org"));
		vSeeds.push_back(CDNSSeedData("seed6", "2001:19f0:5001:2c6b:5400:02ff:fe03:b81c"));
		vSeeds.push_back(CDNSSeedData("seed7", "2001:19f0:5001:3e20:5400:02ff:fe03:b7e4"));
		vSeeds.push_back(CDNSSeedData("seed8", "2001:19f0:6c01:28b0:5400:02ff:fe03:b7f1"));
		vSeeds.push_back(CDNSSeedData("seed9", "2001:19f0:5:1920:5400:02ff:fe03:b7fa"));
		vSeeds.push_back(CDNSSeedData("seed10", "2001:19f0:6801:45e:5400:02ff:fe03:b814"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 63);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 2);
        base58Prefixes[STAKING_ADDRESS] = std::vector<unsigned char>(1, 28); // Starting with 'S'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 212);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x02)(0x2D)(0x25)(0x33).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x02)(0x21)(0x31)(0x2B).convert_to_container<std::vector<unsigned char> >();
        // BIP44 coin type is from https://github.com/satoshilabs/slips/blob/master/slip-0044.md
        nExtCoinType = 188;

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return data;
    }

};
static CMainParams mainParams;

/**
 * Schilling Coin (SCH) Testnet (v3)
 */
class CTestNetParams : public CMainParams
{
public:
    CTestNetParams()
    {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";

        genesis = CreateGenesisBlock(1538323043, 1050765, 0x1e0ffff0, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        //assert(consensus.hashGenesisBlock == uint256("0000024c78d7d2fb56363f7777bab06de80307ac751b02e843ca7ae62d2310d2"));

        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.powLimit   = ~UINT256_ZERO >> 20; // SchillingCoin starting difficulty is 1 / 2^12.
        consensus.posLimitV1 = ~UINT256_ZERO >> 24;
        consensus.posLimitV2 = ~UINT256_ZERO >> 20;
        consensus.nBudgetCycleBlocks = 144; // Approx 10 cycles per day.
        consensus.nBudgetFeeConfirmations = 3; // (Only 8-blocks window for finalization on testnet)
        consensus.nCoinbaseMaturity = 15;
        consensus.nFutureTimeDriftPoW = 7200;
        consensus.nFutureTimeDriftPoS = 180;
        consensus.nMasternodeCountDrift = 4; // Number of MN we allow the see-saw payments to be off by.
        consensus.nMaxMoneyOut = 83000000 * COIN;
        consensus.nPoolMaxTransactions = 2;
        consensus.nProposalEstablishmentTime = 60 * 5; // At least 5 min old to make it into a budget.
        consensus.nStakeMinAge = 60 * 60;
        consensus.nStakeMinDepth = 100;
        consensus.nTargetTimespan = 40 * 60;
        consensus.nTargetTimespanV2 = 30 * 60;
        consensus.nTargetSpacing = 1 * 60;
        consensus.nTimeSlotLength = 15;
        consensus.strObfuscationPoolDummyAddress = "y57cqfGRkekRyDRNeJiLtYVEbvhXrNbmox";

        // Height Based Activations
        consensus.height_last_PoW = 200;
        consensus.height_last_ZC_AccumCheckpoint = 1106090;
        consensus.height_start_BIP65 = 851019;
        consensus.height_start_MessSignaturesV2 = 1347000; // TimeProtocolV2, Blocks V7 and newMessageSignatures
        consensus.height_start_StakeModifierNewSelection = 51197;
        consensus.height_start_StakeModifierV2 = 1214000;
        consensus.height_start_TimeProtoV2 = 1347000; // TimeProtocolV2, Blocks V7 and newMessageSignatures
        consensus.height_start_ZC = 201576;
        consensus.height_start_ZC_PublicSpends = 1106100;
        consensus.height_start_ZC_SerialRangeCheck = 1;
        consensus.height_start_ZC_SerialsV2 = 444020;

        // Zerocoin-related params
        consensus.ZC_Modulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
                "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
                "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
                "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
                "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
                "31438167899885040445364023527381951378636564391212010397122822120720357";
        consensus.ZC_MaxPublicSpendsPerTx = 637; // Assume about 220 bytes each input
        consensus.ZC_MaxSpendsPerTx = 7; // Assume about 20kb each input
        consensus.ZC_MinMintConfirmations = 20;
        consensus.ZC_MinMintFee = 1 * CENT;
        consensus.ZC_MinStakeDepth = 200;
        consensus.ZC_TimeStart = 1501776000;

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */

        pchMessageStart[0] = 0x45;
        pchMessageStart[1] = 0x76;
        pchMessageStart[2] = 0x65;
        pchMessageStart[3] = 0xba;
        nDefaultPort = 51474;
        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.push_back(CDNSSeedData("fuzzbawls.pw", "schillingcoin-testnet.seed.fuzzbawls.pw", true));
        vSeeds.push_back(CDNSSeedData("fuzzbawls.pw", "schillingcoin-testnet.seed2.fuzzbawls.pw", true));
        vSeeds.push_back(CDNSSeedData("warrows.dev", "testnet.dnsseed.schillingcoin.warrows.dev"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 139); // Testnet SchillingCoin addresses start with 'x' or 'y'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);  // Testnet SchillingCoin script addresses start with '8' or '9'
        base58Prefixes[STAKING_ADDRESS] = std::vector<unsigned char>(1, 73);     // starting with 'W'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);     // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        // Testnet SchillingCoin BIP32 pubkeys start with 'DRKV'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x3a)(0x80)(0x61)(0xa0).convert_to_container<std::vector<unsigned char> >();
        // Testnet SchillingCoin BIP32 prvkeys start with 'DRKP'
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x3a)(0x80)(0x58)(0x37).convert_to_container<std::vector<unsigned char> >();
        // Testnet SchillingCoin BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams
{
public:
    CRegTestParams()
    {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";

        genesis = CreateGenesisBlock(1454124731, 2402015, 0x1e0ffff0, 1, 250 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // assert(consensus.hashGenesisBlock == uint256("0x0000041e482b9b9691d98eefb48473405c0b8ec31b76df3797c74a78680ef818"));
        // assert(genesis.hashMerkleRoot == uint256("0x1b2ef6e2f28be914103a277377ae7729dcd125dfeb8bf97bd5964ba72b6dc39b"));

        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.powLimit   = ~UINT256_ZERO >> 20; // SchillingCoin starting difficulty is 1 / 2^12.
        consensus.posLimitV1 = ~UINT256_ZERO >> 24;
        consensus.posLimitV2 = ~UINT256_ZERO >> 20;
        consensus.nBudgetCycleBlocks = 144; // Approx 10 cycles per day.
        consensus.nBudgetFeeConfirmations = 3; // (Only 8-blocks window for finalization on regtest).
        consensus.nCoinbaseMaturity = 100;
        consensus.nFutureTimeDriftPoW = 7200;
        consensus.nFutureTimeDriftPoS = 180;
        consensus.nMasternodeCountDrift = 4; // Num of MN we allow the see-saw payments to be off by.
        consensus.nMaxMoneyOut = 43199500 * COIN;
        consensus.nPoolMaxTransactions = 2;
        consensus.nProposalEstablishmentTime = 60 * 5; // At least 5 min old to make it into a budget.
        consensus.nStakeMinAge = 0;
        consensus.nStakeMinDepth = 2;
        consensus.nTargetTimespan = 40 * 60;
        consensus.nTargetTimespanV2 = 30 * 60;
        consensus.nTargetSpacing = 1 * 60;
        consensus.nTimeSlotLength = 15;
        consensus.strObfuscationPoolDummyAddress = "y57cqfGRkekRyDRNeJiLtYVEbvhXrNbmox";

        // height based activations
        consensus.height_last_PoW = 250;
        consensus.height_last_ZC_AccumCheckpoint = 310; // No checkpoints on regtest.
        consensus.height_start_BIP65 = 851019; // Not defined for regtest. Inherit TestNet value.
        consensus.height_start_MessSignaturesV2 = 1;
        consensus.height_start_StakeModifierNewSelection = 0;
        consensus.height_start_StakeModifierV2 = 251; // Atart with modifier V2 on regtest.
        consensus.height_start_TimeProtoV2 = 999999999;
        consensus.height_start_ZC = 300;
        consensus.height_start_ZC_PublicSpends = 400;
        consensus.height_start_ZC_SerialRangeCheck = 300;
        consensus.height_start_ZC_SerialsV2 = 300;

        // Zerocoin-related params
        consensus.ZC_Modulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
                "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
                "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
                "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
                "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
                "31438167899885040445364023527381951378636564391212010397122822120720357";
        consensus.ZC_MaxPublicSpendsPerTx = 637; // Assume about 220 bytes each input
        consensus.ZC_MaxSpendsPerTx = 7; // Assume about 20kb each input
        consensus.ZC_MinMintConfirmations = 10;
        consensus.ZC_MinMintFee = 1 * CENT;
        consensus.ZC_MinStakeDepth = 10;
        consensus.ZC_TimeStart = 0; // Not implemented on regtest.


        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */

        pchMessageStart[0] = 0xa1;
        pchMessageStart[1] = 0xcf;
        pchMessageStart[2] = 0x7e;
        pchMessageStart[3] = 0xac;
        nDefaultPort = 51476;

        vFixedSeeds.clear(); //! Testnet mode doesn't have any fixed seeds.
        vSeeds.clear(); //! Testnet mode doesn't have any DNS seeds.
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

static CChainParams* pCurrentParams = 0;

const CChainParams& Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(CBaseChainParams::Network network)
{
    switch (network) {
    case CBaseChainParams::MAIN:
        return mainParams;
    case CBaseChainParams::TESTNET:
        return testNetParams;
    case CBaseChainParams::REGTEST:
        return regTestParams;
    default:
        assert(false && "Unimplemented network");
        return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}