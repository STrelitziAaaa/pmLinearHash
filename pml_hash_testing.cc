#include "pml_hash.h"

PMLHash* TestHashConstruct() {
  PMLHash* f = new PMLHash("./pmemFile");
  f->showPrivateData();
  return f;
}

int TestHashInsert(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE; i++) {
    if (f->insert(i, i) != -1) {
      printf("|Insert| key:%zu,val:%zu\n", i, i);
    } else {
      printf("|Insert| key:%zu,val:%zu InsertErr\n", i, i);
    }
  }

  uint64_t i = 1;
  if (f->insert(i, i) != -1) {
    printf("|Insert| key:%zu,val:%zu\n", i, i);
  } else {
    printf("|Insert| key:%zu,val:%zu InsertErr\n", i, i);
  }
  printf("After Insert:\n");
  f->showKV();
}

int TestHashSearch(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE + 10; i += 5) {
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
  for (uint64_t i = 1; i <= HASH_SIZE + 10; i += 5) {
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
  for (uint64_t i = 1; i <= HASH_SIZE + 10; i += 5) {
    if (f->remove(i) == -1) {
      printf("|Remove| key:%zu Not Found\n", i);
    } else {
      printf("|Remove| key:%zu remove ok\n", i);
    }
  }
  f->showKV();
}

int TestBitMap(PMLHash* f) {
  bitmap_st* bm = f->getBitMapForTest();
  bm->init();
  f->showBitMap();
  bm->set(0);
  bm->set(1);
  f->showBitMap();
  printf("first bit pos:%d\n", bm->findFirstAvailable());
  bm->reset(0);
  f->showBitMap();
  printf("first bit pos:%d\n", bm->findFirstAvailable());
}

int main() {
  // you have to return the original object,instead of use `=` in
  // TestHashConstruct(&f) because it will call `unmap` if it die;
  PMLHash* f = TestHashConstruct();
  f->showBitMap();
  TestHashInsert(f);
  TestHashSearch(f);
  TestHashUpdate(f);
  TestHashRemove(f);
  f->showBitMap();

  f->remove(15);
  f->showKV();
  f->showBitMap();
}