#include "pml_hash.h"

PMLHash* TestHashConstruct() {
  PMLHash* f = new PMLHash("./pmemFile");
  f->showPrivateData();
  return f;
}

int TestHashInsert(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE; i++) {
    f->insert(i, i);
  }
  f->showKV();
}

int TestHashSearch(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE + 5; i++) {
    uint64_t val;
    // you may need to check returnValue != -1
    f->search(i, val);
    printf("|Search Result| key:%zu value:%zu\n", i, val);
  }
}

int main() {
  // todo: 为什么使用 pmlhash f, 传&f 不行?
  PMLHash* f = TestHashConstruct();
  TestHashInsert(f);
  TestHashSearch(f);
}