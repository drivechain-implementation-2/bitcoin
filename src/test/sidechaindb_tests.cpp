// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "main.h"
#include "miner.h"
#include "script/sigcache.h"
#include "sidechaindb.h"
#include "uint256.h"
#include "utilstrencodings.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(sidechaindb_tests, TestChain100Setup)

//std::vector<CMutableTransaction> CreateDepositTransactions()
//{
//    std::vector<CMutableTransaction> depositTransactions;
//    depositTransactions.resize(3);

//    // Create some dummy input transactions
//    depositTransactions[0].vout.resize(1);
//    depositTransactions[0].vout[0].nValue = 50*CENT;
//    depositTransactions[0].vout[0].scriptPubKey = ValidSidechains[SIDECHAIN_TEST].depositScript;

//    depositTransactions[1].vout.resize(1);
//    depositTransactions[1].vout[0].nValue = 21*CENT;
//    depositTransactions[1].vout[0].scriptPubKey = ValidSidechains[SIDECHAIN_HIVEMIND].depositScript;

//    depositTransactions[2].vout.resize(1);
//    depositTransactions[2].vout[0].nValue = 71*CENT;
//    depositTransactions[2].vout[0].scriptPubKey = ValidSidechains[SIDECHAIN_WIMBLE].depositScript;

//    return depositTransactions;
//}

//CTransaction CreateWTJoin()
//{
//    CMutableTransaction wtj;
//    wtj.vout.resize(1);
//    wtj.vout[0].nValue = 50*CENT;
//    wtj.vout[0].scriptPubKey = ValidSidechains[SIDECHAIN_TEST].depositScript;

//    return wtj;
//}

//CBlock CreateBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
//{
//    const CChainParams& chainparams = Params();
//    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
//    CBlock& block = pblocktemplate->block;

//    // Replace mempool-selected txns with just coinbase plus passed-in txns:
//    block.vtx.resize(1);
//    for (const CMutableTransaction& tx : txns) {
//        block.vtx.push_back(MakeTransactionRef(tx));
//    }

//    // IncrementExtraNonce creates a valid coinbase and merkleRoot
//    unsigned int extraNonce = 0;
//    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

//    while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

//    CBlock result = block;
//    return result;
//}

//bool ProcessBlock(const CBlock &block)
//{
//    const CChainParams& chainparams = Params();
//    return ProcessNewBlock(chainparams, &block, true, NULL, NULL);
//}

//BOOST_AUTO_TEST_CASE(sidechaindb_blockchain)
//{
//    // TODO

//    // Create deposit
//    CMutableTransaction depositTX;
//    depositTX.vin.resize(1);
//    depositTX.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
//    depositTX.vin[0].prevout.n = 0;
//    depositTX.vout.resize(1);
//    depositTX.vout[0].nValue = 50 * CENT;
//    depositTX.vout[0].scriptPubKey = ValidSidechains[SIDECHAIN_TEST].depositScript;

//    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

//    std::vector<unsigned char> vchSig;
//    uint256 hash = SignatureHash(scriptPubKey, depositTX, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
//    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
//    vchSig.push_back((unsigned char)SIGHASH_ALL);
//    depositTX.vin[0].scriptSig << vchSig;

//    std::vector<CMutableTransaction> vDeposit;
//    vDeposit.push_back(depositTX);

//    // Add deposit to blockchain
//    CBlock depositBlock = CreateBlock(vDeposit, scriptPubKey);

//    BOOST_CHECK(ProcessBlock(depositBlock));

//    // TODO need to actually check height...

//    // Create WT^ (try to spend the deposit created in the previous block)
//    CMutableTransaction wtx;
//    wtx.vin.resize(1);
//    wtx.vin[0].prevout.hash = depositTX.GetHash();
//    wtx.vin[0].prevout.n = 0;
//    wtx.vout.resize(1);
//    wtx.vout[0].nValue = 42 * CENT;
//    wtx.vout[0].scriptPubKey = scriptPubKey;

//    std::vector<CMutableTransaction> vWT;
//    vWT.push_back(wtx);

//    CBlock spendBlock = CreateBlock(vWT, scriptPubKey);

//    // Workscore should be invalid right now
//    BOOST_CHECK(ProcessBlock(spendBlock));

//    // Make workscore valid
//    for (unsigned int i = 0; i < 200; i++) {
//        CMutableTransaction verificationTX;
//        // TODO create verifications
//        std::vector<CMutableTransaction> txns;
//        // CreateAndProcessBlock(txns, scriptPubKey);
//    }
//}

//BOOST_AUTO_TEST_CASE(sidechaindb_isolated)
//{
//    // Test SidechainDB without blocks
//    SidechainDB scdb;

//    const Sidechain& test = ValidSidechains[SIDECHAIN_TEST];
//    const Sidechain& hivemind = ValidSidechains[SIDECHAIN_HIVEMIND];
//    const Sidechain& wimble = ValidSidechains[SIDECHAIN_WIMBLE];

//    int blocksLeft0 = test.nWaitPeriod + test.nVerificationPeriod;
//    int blocksLeft1 = hivemind.nWaitPeriod + hivemind.nVerificationPeriod;
//    int blocksLeft2 = wimble.nWaitPeriod + wimble.nVerificationPeriod;

//    int score0, score1, score2;
//    score0 = score1 = score2 = 0;

//    uint256 wtxid0, wtxid1, wtxid2;
//    wtxid0 = wtxid1 = wtxid2 = CTransaction().GetHash();

//    for (int i = 0; i <= 200; i++) {
//        scdb.Update(0, blocksLeft0, score0, wtxid0);
//        scdb.Update(1, blocksLeft1, score1, wtxid1);
//        scdb.Update(2, blocksLeft2, score2, wtxid2);

//        score0++;

//        if (i % 2 == 0)
//            score1++;

//        blocksLeft0--;
//        blocksLeft1--;
//        blocksLeft2--;
//    }

//    // WT^ 0 should pass with valid workscore (200/100)
//    BOOST_CHECK(scdb.CheckWorkScore(0, wtxid0));
//    // WT^ 1 should fail with unsatisfied workscore (100/200)
//    BOOST_CHECK(!scdb.CheckWorkScore(1, wtxid1));
//    // WT^ 2 should fail with unsatisfied workscore (0/200)
//    BOOST_CHECK(!scdb.CheckWorkScore(2, wtxid2));
//}

//BOOST_AUTO_TEST_CASE(sidechaindb_CreateStateScript)
//{
//    // Verify that state scripts created based on known SidechainDB
//    // state examples are formatted as expected

//    std::vector<CMutableTransaction> vDepositTx = CreateDepositTransactions();
//    BOOST_REQUIRE(vDepositTx.size() == 3);
//    std::vector<unsigned char> data;

//    //Empty DB test
//    SidechainDB dbEmpty;
//    CScript empty = dbEmpty.CreateStateScript();
//    data.assign(empty.begin(), empty.begin() + empty.size());
//    std::string strEmpty(data.begin(), data.end());

//    BOOST_CHECK(strEmpty == "");

//    // Sparsely populated DB test
//    std::string populatedExpected = "0:1|1|1";
//    SidechainDB dbPopulated;
//    dbPopulated.Update(0, 400, 0, vDepositTx[0].GetHash());
//    dbPopulated.Update(0, 399, 1, vDepositTx[0].GetHash());
//    dbPopulated.Update(1, 398, 0, vDepositTx[0].GetHash());
//    dbPopulated.Update(2, 397, 0, vDepositTx[0].GetHash());
//    dbPopulated.Update(2, 396, 1, vDepositTx[0].GetHash());
//    dbPopulated.Update(2, 396, 2, vDepositTx[0].GetHash());

//    CScript populated = dbPopulated.CreateStateScript();
//    data.clear();
//    data.assign(populated.begin(), populated.end());
//    BOOST_REQUIRE(data.size() > 5);
//    std::string strPopulated(data.begin() + 2, data.end());

//    BOOST_CHECK(strPopulated == populatedExpected);

//    // Filled DB test
//    std::string fullExpected = "0:1.0.0|1.0.0|1.0.0";
//    SidechainDB dbFull;
//    int score1, score2, score3;
//    score1 = score2 = score3 = 0;
//    for (int i = 400; i >= 0; i--) {
//        dbFull.Update(SIDECHAIN_TEST, i, score1, vDepositTx[0].GetHash());
//        dbFull.Update(SIDECHAIN_HIVEMIND, i, score1, vDepositTx[0].GetHash());
//        dbFull.Update(SIDECHAIN_WIMBLE, i, score1, vDepositTx[0].GetHash());

//        dbFull.Update(SIDECHAIN_TEST, i, score2, vDepositTx[1].GetHash());
//        dbFull.Update(SIDECHAIN_HIVEMIND, i, score2, vDepositTx[1].GetHash());
//        dbFull.Update(SIDECHAIN_WIMBLE, i, score2, vDepositTx[1].GetHash());

//        dbFull.Update(SIDECHAIN_TEST, i, score3, vDepositTx[2].GetHash());
//        dbFull.Update(SIDECHAIN_HIVEMIND, i, score3, vDepositTx[2].GetHash());
//        dbFull.Update(SIDECHAIN_WIMBLE, i, score3, vDepositTx[2].GetHash());

//        score1++;

//        if (i % 2 == 0)
//            score2++;
//        if (i % 3 == 0)
//            score3++;
//    }

//    CScript full = dbFull.CreateStateScript();
//    data.clear();
//    data.assign(full.begin(), full.end());
//    BOOST_REQUIRE(data.size() > 5);
//    std::string strFull(data.begin() + 2, data.end());

//    BOOST_CHECK(strFull == fullExpected);

//    // TODO test with different number of WT^s per sidechain
//    // TODO test with some sidechains having WT^s and some not
//}

//BOOST_AUTO_TEST_CASE(sidechaindb_Update)
//{
//    // TODO

//    // Valid update (upvotes), should work

//    // Invalid increment, should be rejected

//    // Invalid sidechain, should be rejected

//    // Valid update (downvotes), should work
//}

BOOST_AUTO_TEST_SUITE_END()
