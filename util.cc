#include "util.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include "pml_hash.h"

bool chkAndCrtFile(const char* filePath) {
  struct stat fstate;
  if (!stat(filePath, &fstate)) {
    printf("file exists\n");
    return true;
  } else {
    if (errno == ENOENT) {
      // note: the file mode may be set as 0644 even if we give 0666
      // todo: here we need to mkdir
      close(creat(filePath, 0666));
      printf("file created\n");
      return false;
    } else {
      perror("stat");
    }
  }
}

constexpr uint64_t INSERT = 1;
constexpr uint64_t READ = 2;
map<std::string, uint64_t> TypeDict = {{"INSERT", INSERT}, {"READ", READ}};

int runYCSBBenchmark(const char* filePath, PMLHash* f) {
  std::ifstream in_(filePath);
  std::string typeStr;
  uint64_t key;
  double total;

  auto t_start = std::chrono::high_resolution_clock::now();
  while (!in_.eof()) {
    in_ >> typeStr >> key;
    if (typeStr == "INSERT") {
      f->insert(key, key);

    } else {
      uint64_t value;
      assert(f->search(key, value) != -1);
      assert(value == key);
    }
  }
  auto t_end = std::chrono::high_resolution_clock::now();
  std::cout
      << "run " << filePath << " | WallTime: "
      << std::chrono::duration<double, std::milli>(t_end - t_start).count()
      << " ms" << std::endl;
  return 0;
}

int loadYCSBBenchmark(const char* filePath, PMLHash* f) {
  std::ifstream in_(filePath);
  std::string typeStr;
  uint64_t key;
  double total;
  auto t_start = std::chrono::high_resolution_clock::now();
  while (!in_.eof()) {
    in_ >> typeStr >> key;
    // std::cout << "insert " << key << std::endl;
    f->insert(key, key);
  }
  auto t_end = std::chrono::high_resolution_clock::now();
  std::cout
      << "load " << filePath << " | WallTime: "
      << std::chrono::duration<double, std::milli>(t_end - t_start).count()
      << " ms" << std::endl;
  return 0;
}

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

int AssertTEST(PMLHash* f) {
  const int N = 200000;

  printf("=========Assert Test==========\n");
  printf("Prepare %d KVs\n", N);

  for (int i = 0; i < N; i++) {
    uint64_t value;
    assert(f->search(i, value) == -1);
    assert(f->update(i, value) == -1);
    assert(f->remove(i) == -1);
  }

  // insert
  for (int i = 0; i < N; i++) {
    assert(f->insert(i, i) != -1);
    assert(f->insert(i, i) == -1);
    uint64_t value = N;
    assert(f->search(i, value) != -1);
    assert(i == value);
  }
  // search again
  for (int i = 0; i < N; i++) {
    uint64_t value = N;
    assert(f->search(i, value) != -1);
    assert(i == value);
  }
  // update
  for (int i = 0; i < N; i++) {
    uint64_t value;
    assert(f->update(i, i + 1) != -1);
    assert(f->search(i, value) != -1);
    assert(value == i + 1);
  }
  // search again
  for (int i = 0; i < N; i++) {
    uint64_t value;
    assert(f->search(i, value) != -1);
    assert(value == i + 1);
  }
  // remove
  for (int i = 0; i < N; i++) {
    assert(f->remove(i) != -1);
    assert(f->remove(i) == -1);
    uint64_t value;
    assert(f->search(i, value) == -1);
  }
  // search again
  for (int i = 0; i < N; i++) {
    uint64_t value;
    assert(f->search(i, value) == -1);
  }
  printf("=========Assert OK============\n");
}

int BenchmarkYCSB(PMLHash* f) {
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