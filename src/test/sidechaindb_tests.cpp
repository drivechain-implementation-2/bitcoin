// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdlib.h>

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

//! KeyID for testing
// mx3PT9t2kzCFgAURR9HeK6B5wN8egReUxY
// cN5CqwXiaNWhNhx3oBQtA8iLjThSKxyZjfmieTsyMpG6NnHBzR7J
static const std::string testkey = "b5437dc6a4e5da5597548cf87db009237d286636";


BOOST_FIXTURE_TEST_SUITE(sidechaindb_tests, TestChain100Setup)

std::vector<CMutableTransaction> CreateDepositTransactions(SidechainNumber sidechain, int count)
{
    std::vector<CMutableTransaction> depositTransactions;
    depositTransactions.resize(count);
    for (int i = 0; i < count; i++) {
        // Random enough output value
        int rand = (std::rand() % 50) + i;

        depositTransactions[i].vout.resize(1);
        depositTransactions[i].vout[0].nValue = rand*CENT;
        depositTransactions[i].vout[0].scriptPubKey = CScript() << sidechain << ToByteVector(testkey) << OP_NOP4;
    }
    return depositTransactions;
}

CBlock CreateBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
    const CChainParams& chainparams = Params();
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
    CBlock& block = pblocktemplate->block;

    // Replace mempool-selected txns with just coinbase plus passed-in txns:
    block.vtx.resize(1);
    for (const CMutableTransaction& tx : txns) {
        block.vtx.push_back(MakeTransactionRef(tx));
    }

    // IncrementExtraNonce creates a valid coinbase and merkleRoot
    unsigned int extraNonce = 0;
    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

    while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus()))
        ++block.nNonce;

    CBlock result = block;
    return result;
}

bool ProcessBlock(const CBlock &block)
{
    const CChainParams& chainparams = Params();
    return ProcessNewBlock(chainparams, &block, true, NULL, NULL);
}

BOOST_AUTO_TEST_CASE(sidechaindb_blockchain)
{
    // Test SCDB with blockchain

    // Create deposit
    CMutableTransaction depositTX;
    depositTX.vin.resize(1);
    depositTX.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    depositTX.vin[0].prevout.n = 0;
    depositTX.vout.resize(1);
    depositTX.vout[0].nValue = 50 * CENT;
    depositTX.vout[0].scriptPubKey = CScript() << SIDECHAIN_TEST << ToByteVector(testkey) << OP_NOP4;

    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    std::vector<unsigned char> vchSig;
    uint256 hash = SignatureHash(scriptPubKey, depositTX, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    depositTX.vin[0].scriptSig << vchSig;

    // Add deposit to blockchain
    std::vector<CMutableTransaction> vDeposit;
    vDeposit.push_back(depositTX);
    CBlock depositBlock = CreateBlock(vDeposit, scriptPubKey);

    int preDepositHeight = chainActive.Height();
    BOOST_CHECK(ProcessBlock(depositBlock));
    BOOST_REQUIRE(chainActive.Height() == (preDepositHeight + 1));

    // Create WT^ (try to spend the deposit created in the previous block)
    CMutableTransaction wtx;
    wtx.vin.resize(1);
    wtx.vin[0].prevout.hash = depositTX.GetHash();
    wtx.vin[0].prevout.n = 0;
    wtx.vout.resize(1);
    wtx.vout[0].nValue = 42 * CENT;
    wtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(testkey) << OP_CHECKSIGVERIFY;

    // Add WT^ to blockchain
    std::vector<CMutableTransaction> vWT;
    vWT.push_back(wtx);
    CBlock spendBlock = CreateBlock(vWT, scriptPubKey);

    // Workscore should be invalid right now
    int heightPreSpend = chainActive.Height();
    ProcessBlock(spendBlock);
    BOOST_REQUIRE(chainActive.Height() == heightPreSpend);

    // TODO
    // Manully submit WT^ to SCDB for tracking and verification
    // BOOST_REQUIRE(scdb.AddSidechainWTJoin(SIDECHAIN_TEST, wtx));

    // Make workscore valid
    int heightPreVote = chainActive.Height();
    for (unsigned int i = 0; i < 100; i++) {
        // Generate a new coinbase if we need to
        if ((i + 1) == coinbaseTxns.size()) {
            std::vector<CMutableTransaction> noTxns;
            CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
            coinbaseTxns.push_back(*b.vtx[0]);
        }

        CMutableTransaction verificationTX;
        verificationTX.vin.resize(1);
        verificationTX.vin[0].prevout.hash = coinbaseTxns[i].GetHash();
        verificationTX.vin[0].prevout.n = 0;
        verificationTX.vout.resize(1);
        verificationTX.vout[0].nValue = CENT;
        verificationTX.vout[0].scriptPubKey = scdb.CreateStateScript();

        std::vector<unsigned char> vchSigSpend;
        uint256 hashSpend = SignatureHash(scriptPubKey, verificationTX, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
        coinbaseKey.Sign(hashSpend, vchSigSpend);
        vchSigSpend.push_back((unsigned char)SIGHASH_ALL);
        verificationTX.vin[0].scriptSig << vchSigSpend;

        std::vector<CMutableTransaction> txns;
        txns.push_back(verificationTX);

        CreateAndProcessBlock(txns, scriptPubKey);
    }

    int heightPreWTJoin = chainActive.Height();
    BOOST_REQUIRE(heightPreWTJoin == (heightPreVote + 100));

    const Sidechain &sidechainTest = ValidSidechains[SIDECHAIN_TEST];
    uint16_t nTau = sidechainTest.nWaitPeriod + sidechainTest.nVerificationPeriod;
    for (int i = heightPreWTJoin; i < nTau; i++) {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        coinbaseTxns.push_back(*b.vtx[0]);
    }

    BOOST_REQUIRE(chainActive.Height() == nTau);
}

BOOST_AUTO_TEST_CASE(sidechaindb_isolated)
{
    std::vector<CMutableTransaction> vTestDeposit = CreateDepositTransactions(SIDECHAIN_TEST, 1);
    std::vector<CMutableTransaction> vHivemindDeposit = CreateDepositTransactions(SIDECHAIN_HIVEMIND, 1);
    std::vector<CMutableTransaction> vWimbleDeposit = CreateDepositTransactions(SIDECHAIN_WIMBLE, 1);
    BOOST_REQUIRE(vTestDeposit.size() == 1);
    BOOST_REQUIRE(vHivemindDeposit.size() == 1);
    BOOST_REQUIRE(vWimbleDeposit.size() == 1);

    // Test SidechainDB without blocks
    SidechainDB scdb;

    const Sidechain& test = ValidSidechains[SIDECHAIN_TEST];
    const Sidechain& hivemind = ValidSidechains[SIDECHAIN_HIVEMIND];
    const Sidechain& wimble = ValidSidechains[SIDECHAIN_WIMBLE];

    int blocksLeft0 = test.nWaitPeriod + test.nVerificationPeriod;
    int blocksLeft1 = hivemind.nWaitPeriod + hivemind.nVerificationPeriod;
    int blocksLeft2 = wimble.nWaitPeriod + wimble.nVerificationPeriod;

    int score0, score1;
    score0 = score1 = 0;
    for (int i = 0; i <= 200; i++) {
        scdb.Update(SIDECHAIN_TEST, blocksLeft0, score0, vTestDeposit[0].GetHash());
        scdb.Update(SIDECHAIN_HIVEMIND, blocksLeft1, score1, vHivemindDeposit[0].GetHash());
        scdb.Update(SIDECHAIN_WIMBLE, blocksLeft2, 0, vWimbleDeposit[0].GetHash());

        score0++;

        if (i % 2 == 0)
            score1++;

        blocksLeft0--;
        blocksLeft1--;
        blocksLeft2--;
    }

    // WT^ 0 should pass with valid workscore (200/100)
    BOOST_CHECK(scdb.CheckWorkScore(SIDECHAIN_TEST, vTestDeposit[0].GetHash()));
    // WT^ 1 should fail with unsatisfied workscore (100/200)
    BOOST_CHECK(!scdb.CheckWorkScore(SIDECHAIN_HIVEMIND, vHivemindDeposit[0].GetHash()));
    // WT^ 2 should fail with unsatisfied workscore (0/200)
    BOOST_CHECK(!scdb.CheckWorkScore(SIDECHAIN_WIMBLE, vWimbleDeposit[0].GetHash()));
}

BOOST_AUTO_TEST_CASE(sidechaindb_CreateStateScript)
{
    // Verify that state scripts created based on known SidechainDB
    // state examples are formatted as expected

    std::vector<CMutableTransaction> vTestDeposit = CreateDepositTransactions(SIDECHAIN_TEST, 3);
    std::vector<CMutableTransaction> vHivemindDeposit = CreateDepositTransactions(SIDECHAIN_HIVEMIND, 3);
    std::vector<CMutableTransaction> vWimbleDeposit = CreateDepositTransactions(SIDECHAIN_WIMBLE, 3);
    BOOST_REQUIRE(vTestDeposit.size() == 3);
    BOOST_REQUIRE(vHivemindDeposit.size() == 3);
    BOOST_REQUIRE(vWimbleDeposit.size() == 3);

    // Test empty SCDB
    CScript scriptEmptyExpected = CScript();
    SidechainDB scdbEmpty;

    BOOST_CHECK(scriptEmptyExpected == scdbEmpty.CreateStateScript());

    // Test populated (but not full) SCDB test
    CScript scriptPopulatedExpected;
    scriptPopulatedExpected << OP_RETURN << SCOP_VERSION << SCOP_VERSION_DELIM
                    << SCOP_VERIFY << SCOP_SC_DELIM
                    << SCOP_VERIFY << SCOP_SC_DELIM
                    << SCOP_VERIFY;
    SidechainDB scdbPopulated;
    scdbPopulated.Update(SIDECHAIN_TEST, 400, 0, vTestDeposit[0].GetHash());
    scdbPopulated.Update(SIDECHAIN_TEST, 399, 1, vTestDeposit[0].GetHash());
    scdbPopulated.Update(SIDECHAIN_HIVEMIND, 400, 0, vHivemindDeposit[0].GetHash());
    scdbPopulated.Update(SIDECHAIN_WIMBLE, 400, 0, vWimbleDeposit[0].GetHash());
    scdbPopulated.Update(SIDECHAIN_WIMBLE, 399, 1, vWimbleDeposit[0].GetHash());
    scdbPopulated.Update(SIDECHAIN_WIMBLE, 398, 2, vWimbleDeposit[0].GetHash());

    BOOST_CHECK(scriptPopulatedExpected == scdbPopulated.CreateStateScript());

    // Test Full SCDB
    CScript scriptFullExpected;
    scriptFullExpected << OP_RETURN << SCOP_VERSION << SCOP_VERSION_DELIM
               << SCOP_VERIFY << SCOP_WT_DELIM << SCOP_REJECT << SCOP_WT_DELIM << SCOP_REJECT
               << SCOP_SC_DELIM
               << SCOP_VERIFY << SCOP_WT_DELIM << SCOP_REJECT << SCOP_WT_DELIM << SCOP_REJECT
               << SCOP_SC_DELIM
               << SCOP_VERIFY << SCOP_WT_DELIM << SCOP_REJECT << SCOP_WT_DELIM << SCOP_REJECT;

    SidechainDB scdbFull;
    // Fill up the state space available to SIDECHAIN_TEST
    scdbFull.Update(SIDECHAIN_TEST, 400, 0, vTestDeposit[0].GetHash());
    scdbFull.Update(SIDECHAIN_TEST, 399, 0, vTestDeposit[1].GetHash());
    scdbFull.Update(SIDECHAIN_TEST, 398, 0, vTestDeposit[2].GetHash());
    // Fill up the state space available to SIDECHAIN_HIVEMIND
    scdbFull.Update(SIDECHAIN_HIVEMIND, 400, 0, vHivemindDeposit[0].GetHash());
    scdbFull.Update(SIDECHAIN_HIVEMIND, 399, 0, vHivemindDeposit[1].GetHash());
    scdbFull.Update(SIDECHAIN_HIVEMIND, 398, 0, vHivemindDeposit[2].GetHash());
    // Fill up the state space available to SIDECHAIN_WIMBLE
    scdbFull.Update(SIDECHAIN_WIMBLE, 400, 0, vWimbleDeposit[0].GetHash());
    scdbFull.Update(SIDECHAIN_WIMBLE, 399, 0, vWimbleDeposit[1].GetHash());
    scdbFull.Update(SIDECHAIN_WIMBLE, 398, 0, vWimbleDeposit[2].GetHash());
    // Upvote the WT^ in state positon 0 for each sidechain
    scdbFull.Update(SIDECHAIN_TEST, 397, 1, vTestDeposit[0].GetHash());
    scdbFull.Update(SIDECHAIN_HIVEMIND, 397, 1, vHivemindDeposit[0].GetHash());
    scdbFull.Update(SIDECHAIN_WIMBLE, 397, 1, vWimbleDeposit[0].GetHash());

    BOOST_CHECK(scriptFullExpected == scdbFull.CreateStateScript());

    // Test with different number of WT^s per sidechain
    CScript scriptWTCountExpected;
    scriptWTCountExpected << OP_RETURN << SCOP_VERSION << SCOP_VERSION_DELIM
                          << SCOP_VERIFY
                          << SCOP_SC_DELIM
                          << SCOP_REJECT << SCOP_WT_DELIM << SCOP_VERIFY
                          << SCOP_SC_DELIM
                          << SCOP_REJECT << SCOP_WT_DELIM << SCOP_VERIFY << SCOP_WT_DELIM << SCOP_REJECT;
    SidechainDB scdbCount;
    scdbCount.Update(SIDECHAIN_TEST, 400, 0, vTestDeposit[0].GetHash());
    scdbCount.Update(SIDECHAIN_TEST, 399, 1, vTestDeposit[0].GetHash());

    scdbCount.Update(SIDECHAIN_HIVEMIND, 400, 0, vHivemindDeposit[0].GetHash());
    scdbCount.Update(SIDECHAIN_HIVEMIND, 399, 0, vHivemindDeposit[1].GetHash());
    scdbCount.Update(SIDECHAIN_HIVEMIND, 398, 1, vHivemindDeposit[1].GetHash());

    scdbCount.Update(SIDECHAIN_WIMBLE, 400, 0, vWimbleDeposit[0].GetHash());
    scdbCount.Update(SIDECHAIN_WIMBLE, 399, 0, vWimbleDeposit[1].GetHash());
    scdbCount.Update(SIDECHAIN_WIMBLE, 398, 0, vWimbleDeposit[2].GetHash());
    scdbCount.Update(SIDECHAIN_WIMBLE, 397, 1, vWimbleDeposit[1].GetHash());

    BOOST_CHECK(scriptWTCountExpected == scdbCount.CreateStateScript());

    // Test with some sidechains missing WT^ for this period
    CScript scriptMissingWTExpected;
    scriptMissingWTExpected << OP_RETURN << SCOP_VERSION << SCOP_VERSION_DELIM
                            << SCOP_VERIFY
                            << SCOP_SC_DELIM << SCOP_SC_DELIM
                            << SCOP_REJECT;
    SidechainDB scdbMissing;
    // TODO

    // Test WT^ in different position for each sidechain
    CScript scriptWTPositionExpected;
    scriptWTPositionExpected << OP_RETURN << SCOP_VERSION << SCOP_VERSION_DELIM
                             << SCOP_VERIFY << SCOP_WT_DELIM << SCOP_REJECT << SCOP_WT_DELIM << SCOP_REJECT
                             << SCOP_SC_DELIM
                             << SCOP_REJECT << SCOP_WT_DELIM << SCOP_VERIFY << SCOP_WT_DELIM << SCOP_REJECT
                             << SCOP_SC_DELIM
                             << SCOP_REJECT << SCOP_WT_DELIM << SCOP_REJECT << SCOP_WT_DELIM << SCOP_VERIFY;
    SidechainDB scdbPosition;
    scdbPosition.Update(SIDECHAIN_TEST, 400, 0, vTestDeposit[0].GetHash());
    scdbPosition.Update(SIDECHAIN_TEST, 399, 0, vTestDeposit[1].GetHash());
    scdbPosition.Update(SIDECHAIN_TEST, 398, 0, vTestDeposit[2].GetHash());
    scdbPosition.Update(SIDECHAIN_TEST, 397, 1, vTestDeposit[0].GetHash());

    scdbPosition.Update(SIDECHAIN_HIVEMIND, 400, 0, vHivemindDeposit[0].GetHash());
    scdbPosition.Update(SIDECHAIN_HIVEMIND, 399, 0, vHivemindDeposit[1].GetHash());
    scdbPosition.Update(SIDECHAIN_HIVEMIND, 398, 0, vHivemindDeposit[2].GetHash());
    scdbPosition.Update(SIDECHAIN_HIVEMIND, 397, 1, vHivemindDeposit[1].GetHash());

    scdbPosition.Update(SIDECHAIN_WIMBLE, 400, 0, vWimbleDeposit[0].GetHash());
    scdbPosition.Update(SIDECHAIN_WIMBLE, 399, 0, vWimbleDeposit[1].GetHash());
    scdbPosition.Update(SIDECHAIN_WIMBLE, 398, 0, vWimbleDeposit[2].GetHash());
    scdbPosition.Update(SIDECHAIN_WIMBLE, 397, 1, vWimbleDeposit[2].GetHash());

    BOOST_CHECK(scriptWTPositionExpected == scdbPosition.CreateStateScript());
}

//BOOST_AUTO_TEST_CASE(sidechaindb_Update)
//{
//    // TODO

//    // Valid update (upvotes), should work

//    // Invalid increment, should be rejected

//    // Invalid sidechain, should be rejected

//    // Valid update (downvotes), should work
//}

BOOST_AUTO_TEST_SUITE_END()
