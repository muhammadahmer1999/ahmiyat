#include "blockchain.h"
#include <cassert>
#include <iostream>

int main() {
    AhmiyatChain chain;
    Wallet wallet;
    assert(chain.getBalance("genesis", "0") == 100.0);
    Transaction tx(wallet.publicKey, "test", 10.0);
    chain.addPendingTx(tx);
    chain.processPendingTxs();
    assert(chain.assignShard(tx) == chain.assignShard(tx));
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
