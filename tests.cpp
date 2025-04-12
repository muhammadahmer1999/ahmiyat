#include "blockchain.h"
#include <cassert>
#include <iostream>

void testTransactionValidation() {
    Transaction tx("sender", "receiver", 10.0);
    assert(tx.validate() == true);
    Transaction invalidTx("", "receiver", 10.0);
    assert(invalidTx.validate() == false);
    std::cout << "Transaction validation test passed\n";
}

void testMemoryFragment() {
    MemoryFragment mem("text", "memories/test.txt", "Test memory", "owner", 0);
    assert(mem.validate() == true);
    MemoryFragment invalidMem("", "memories/test.txt", "Test memory", "owner", 0);
    assert(invalidMem.validate() == false);
    std::cout << "Memory fragment test passed\n";
}

void testShardManager() {
    ShardManager sm;
    Transaction tx("sender", "receiver", 10.0);
    std::string shardId = sm.assignShard(tx, MAX_SHARDS);
    assert(!shardId.empty());
    sm.updateLoad(shardId, 1);
    std::cout << "Shard manager test passed\n";
}

void testChainBalance() {
    AhmiyatChain chain;
    assert(chain.getBalance("genesis", "0") == 100.0);
    std::cout << "Chain balance test passed\n";
}

void testTransactionCreation() {
    Wallet wallet;
    Transaction tx(wallet.publicKey, "test", 10.0);
    assert(tx.validate() == true);
    std::cout << "Transaction creation test passed\n";
}

int main() {
    testTransactionValidation();
    testMemoryFragment();
    testShardManager();
    testChainBalance();
    testTransactionCreation();
    std::cout << "All tests passed!\n";
    return 0;
}
