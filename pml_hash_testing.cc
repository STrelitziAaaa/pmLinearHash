#include "pml_hash.h"

int TestHashConstruct() {
  PMLHash f("./pmemFile");
  f.showPrivateData();
}

int TestHashInsert() {
  PMLHash f("./pmemFile");
  f.showPrivateData();
  for (uint64_t i = 1; i <= 16; i++) {
    f.insert(i, i);
  }
  f.showKV();
}

int main() {
  // TestHashConstruct();
  TestHashInsert();
}