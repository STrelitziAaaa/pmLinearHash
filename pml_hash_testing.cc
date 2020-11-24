#include <sstream>
#include <thread>
#include "pml_hash.h"
#include "util.h"

PMLHash* TestHashConstruct() {
  PMLHash* f = new PMLHash("./pmemFile");
  f->showConfig();
  return f;
}

int insertWithMsg(PMLHash* f, const uint64_t& key, const uint64_t& value) {
  if (f->insert(key, value) != -1) {
    printf("|Insert| key:%zu,val:%zu\n", key, value);
  } else {
    printf("|Insert| key:%zu,val:%zu InsertErr\n", key, value);
  }
}

int TestHashInsert(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE * 100; i++) {
    uint64_t v = i * (rand() % 2) + rand() % 100;
    insertWithMsg(f, v, v);
  }

  uint64_t dupKey = 200;
  insertWithMsg(f, dupKey, dupKey);
  insertWithMsg(f, dupKey, dupKey);

  f->showKV();
}

int searchWithMsg(PMLHash* f, uint64_t& key, uint64_t& value) {
  if (f->search(key, value) != -1) {
    printf("|Search Result| key:%zu value:%zu\n", key, value);
  } else {
    printf("|Search Result| key:%zu value:Not Found\n", key);
  }
}

int TestHashSearch(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE; i += 5) {
    uint64_t value;
    uint64_t key = i * (rand() % 2) + rand() % 100;
    searchWithMsg(f, key, value);
  }
}

int updateWithMsg(PMLHash* f, uint64_t& key, uint64_t& value) {
  if (f->update(key, value) == -1) {
    printf("|Update| key:%zu Not Found\n", key);
  } else {
    printf("|Update| key:%zu update to %zu\n", key, value);
  }
}

int TestHashUpdate(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE; i += 5) {
    uint64_t key = i * (rand() % 2) + rand() % 100;
    uint64_t value = key + 1;
    updateWithMsg(f, key, value);
  }
  f->showKV();
}

int removeWithMsg(PMLHash* f, uint64_t& key) {
  if (f->remove(key) == -1) {
    printf("|Remove| key:%zu Not Found\n", key);
  } else {
    printf("|Remove| key:%zu remove ok\n", key);
  }
}

int TestHashRemove(PMLHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE + 10; i += 5) {
    uint64_t key = i * (rand() % 2) + rand() % 100;
    removeWithMsg(f, key);
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

int justInsertN(PMLHash* f, int n, uint64_t key) {
  srand(0);
  for (int i = 0; i < n; i++) {
    uint64_t k = key * (rand() % 2) + rand() % 100;
    if (f->insert(k, k) != -1) {
      printf("|Insert| key:%zu,val:%zu\n", k, k);
    } else {
      printf("|Insert| key:%zu,val:%zu InsertErr\n", k, k);
    }
  }
}

int TestMultiThread(PMLHash* f) {
  f->clear();
  f->showKV();

  thread threads[HASH_SIZE * 100];
  for (int i = 0; i < HASH_SIZE * 100; i++) {
    threads[i] = thread(justInsertN, f, 10, i + 1);
  }

  for (auto& i : threads) {
    i.join();
  }
  f->showKV();
  f->showBitMap();
}

int main() {
  // you have to return the original object,instead of use `=` in
  // TestHashConstruct(&f) because it will call `unmap` if it die;

  PMLHash* f = TestHashConstruct();
  // f->showBitMap();
  // TestHashInsert(f);
  // TestHashSearch(f);
  // TestHashUpdate(f);
  // TestHashRemove(f);
  // f->showBitMap();
  // f->remove(15);
  // f->showKV();
  // f->showBitMap();
  // justInsertN(f, 10, 5);
  // TestMultiThread(f);
  loadYCSBBenchmark("./benchmark/10w-rw-0-100-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-0-100-load.txt", f);
  f->clear();
  loadYCSBBenchmark("./benchmark/10w-rw-25-75-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-25-75-load.txt", f);
  f->clear();
  loadYCSBBenchmark("./benchmark/10w-rw-50-50-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-50-50-load.txt", f);
  f->clear();
  loadYCSBBenchmark("./benchmark/10w-rw-75-25-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-75-25-load.txt", f);
  f->clear();
  loadYCSBBenchmark("./benchmark/10w-rw-100-0-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-100-0-load.txt", f);
  f->clear();
}
