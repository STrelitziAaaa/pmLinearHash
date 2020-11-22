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
  printf("After Insert:\n");
  f->showKV();
}

int TestHashSearch(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE + 5; i++) {
    uint64_t val;
    // you may need to check returnValue != -1
    if (f->search(i, val) != -1) {
      printf("|Search Result| key:%zu value:%zu\n", i, val);
    } else {
      printf("|Search Result| key:%zu value:Not Found\n", i);
    }
  }
}

int TestHashUpdate(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE + 5; i++) {
    uint64_t val = i + 1;
    if (f->update(i, val) == -1) {
      printf("|Update| key:%zu Not Found\n", i);
    } else {
      printf("|Update| key:%zu update to %zu\n", i, val);
    }
  }
  f->showKV();
}

int TestHashRemove(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE + 5; i++) {
    if (f->remove(i) == -1) {
      printf("|Remove| key:%zu Not Found\n", i);
    } else {
      printf("|Remove| key:%zu remove ok\n", i);
    }
  }
  f->showKV();
}

int main() {
  // you have to return the original object,instead of use `=` in
  // TestHashConstruct(&f) because it will call `unmap` if it die;
  PMLHash* f = TestHashConstruct();
  TestHashInsert(f);
  TestHashSearch(f);
  TestHashUpdate(f);
  TestHashRemove(f);
}