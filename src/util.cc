#include "util.h"

using namespace std;

static inline void assertOk(int x) {
  assert(x != -1);
}
static inline void assertFail(int x) {
  assert(x == -1);
}

/**
 * @brief check and create file
 * @param filePath
 * @return return -1 if create error; 0 if success
 */
int chkAndCrtFile(const char* filePath) {
  struct stat fstate;
  if (!stat(filePath, &fstate)) {
    printf("pmFile Exists:Recover\n");
    return -1;
  } else {
    if (errno == ENOENT) {
      // note: the file mode may be set as 0644 even if we give 0666
      // todo: here we need to mkdir
      close(creat(filePath, 0666));
      printf("pmFile Created:Init\n");
      return 0;
    }
    perror("stat");
    exit(EXIT_FAILURE);
  }
  return -1;
}

/**
 * @brief load file and benchmark
 * @param filePath
 * @param f the db engine
 * @return nothing
 */
int loadYCSBBenchmark(string filePath, pmLinHash* f) {
  std::ifstream in_(filePath);
  std::string typeStr;
  uint64_t key;
  uint64_t cnt = 0;
  std::vector<uint64_t> keyBuf;
  while (in_.good()) {
    cnt++;
    in_ >> typeStr >> key;
    keyBuf.push_back(key);
  }
  auto t_start = std::chrono::high_resolution_clock::now();
  for (auto& key : keyBuf) {
    f->insert(key, key);
  }
  auto t_end = std::chrono::high_resolution_clock::now();
  auto dur = std::chrono::duration<double, std::milli>(t_end - t_start).count();
  printf("load %s\tWTime: %fms \tOPS: %fM\tDelay: %fns\n", filePath.c_str(),
         dur, cnt / (dur * 1000), dur * 1000 / cnt);
  return 0;
}

/**
 * @brief load file and benchmark
 * @param filePath
 * @param f the db engine
 * @param n_thread the number of thread to run write/read
 * @return
 */
int runYCSBBenchmark(string filePath, pmLinHash* f, uint64_t n_thread) {
  std::ifstream in_(filePath);
  std::string typeStr;
  uint64_t key;
  double cnt = 0;
  // we can't make a insertBuf and searchBuf
  // we should make it come randomly
  std::vector<std::pair<char, uint64_t>> keyBuf;
  while (in_.good()) {
    cnt++;
    in_ >> typeStr >> key;
    keyBuf.push_back(std::make_pair(TypeDict[typeStr], key));
  }
  // split work
  auto t_start = std::chrono::system_clock::now();
  size_t size = keyBuf.size();
  thread threads[n_thread];
  for (size_t i = 0; i < n_thread; i++) {
    threads[i] =
        std::move(thread(thread_routine, std::ref(keyBuf), f, i, n_thread));
  }
  for (auto& i : threads) {
    i.join();
  }
  auto t_end = std::chrono::system_clock::now();
  auto dur = std::chrono::duration<double, std::milli>(t_end - t_start).count();
  printf("run %s\tWTime: %fms \tOPS: %fM\tDelay: %fns\n", filePath.c_str(), dur,
         cnt / (dur * 1000), dur * 1000 / cnt);
  return 0;
}

/**
 * @brief test pmLinear construct
 * @return
 */
pmLinHash* TestHashConstruct() {
  pmLinHash* f = new pmLinHash("./pmemFile");
  f->showConfig();
  return f;
}

/**
 * @brief insert an entry with msg, only be called in TestHashInsert
 * @param f the db engine
 * @param key
 * @param value
 * @return
 */
int insertWithMsg(pmLinHash* f, const uint64_t& key, const uint64_t& value) {
  if (f->insert(key, value) != -1) {
    printf("|Insert| key:%zu,val:%zu\n", key, value);
  } else {
    printf("|Insert| key:%zu,val:%zu InsertErr\n", key, value);
  }
  return 0;
}

/**
 * @brief test pmLinearHash insert
 * @param f the db engine
 * @return
 */
int TestHashInsert(pmLinHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE; i++) {
    insertWithMsg(f, i, i);
  }

  uint64_t dupKey = 200;
  insertWithMsg(f, dupKey, dupKey);
  insertWithMsg(f, dupKey, dupKey);

  f->showKV();
  return 0;
}

/**
 * @brief search an entry with msg, only be called in TestHashSearch
 * @param f the db engine
 * @param key
 * @param value
 * @return
 */
int searchWithMsg(pmLinHash* f, uint64_t& key, uint64_t& value) {
  if (f->search(key, value) != -1) {
    printf("|Search| key:%zu result value:%zu\n", key, value);
  } else {
    printf("|Search| key:%zu value:Not Found\n", key);
  }
  return 0;
}

/**
 * @brief test pmLinearHash search
 * @param f the db engine
 * @return
 */
int TestHashSearch(pmLinHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE; i++) {
    uint64_t value;
    uint64_t key = i;
    searchWithMsg(f, key, value);
  }
  return 0;
}

/**
 * @brief update an entry with msg, only be called in TestHashUpdate
 * @param f the db engine
 * @param key
 * @param value
 * @return
 */
int updateWithMsg(pmLinHash* f, uint64_t& key, uint64_t& value) {
  if (f->update(key, value) == -1) {
    printf("|Update| key:%zu Not Found\n", key);
  } else {
    printf("|Update| key:%zu update to %zu\n", key, value);
  }
  return 0;
}

/**
 * @brief test pmLinearHash update
 * @param f the db engine
 * @return
 */
int TestHashUpdate(pmLinHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE; i++) {
    uint64_t key = i;
    uint64_t value = key + 1;
    updateWithMsg(f, key, value);
  }
  f->showKV();
  return 0;
}

/**
 * @brief remove an entry with msg, only be called in TestHashRemove
 * @param f the db engine
 * @param key
 * @return
 */
int removeWithMsg(pmLinHash* f, uint64_t& key) {
  if (f->remove(key) == -1) {
    printf("|Remove| key:%zu Not Found\n", key);
  } else {
    printf("|Remove| key:%zu remove ok\n", key);
  }
  return 0;
}

/**
 * @brief test pmLinearHash remove
 * @param f the db engine
 * @return
 */
int TestHashRemove(pmLinHash* f) {
  for (uint64_t i = 1; i <= HASH_SIZE; i++) {
    removeWithMsg(f, i);
  }
  f->showKV();
  return 0;
}

/**
 * @brief test bitmap built-in method
 * @param f the db engine, just use nullptr
 * @return
 */
int TestBitMap(pmLinHash* f) {
  bitmap_st* bm = new bitmap_st();
  bm->init();
  f->showBitMap();
  bm->set(0);
  bm->set(1);
  f->showBitMap();
  printf("first bit pos:%lu\n", bm->findFirstAvailable());
  bm->reset(0);
  f->showBitMap();
  printf("first bit pos:%lu\n", bm->findFirstAvailable());
  delete bm;
  return 0;
}

/**
 * @brief insert n entries with msg, only be called in TestMultiThread
 * @param f the db engine
 * @param n insert n entries
 * @param key
 * @param tid the id of the thread
 * @return
 */
int justInsertNWithMsg(pmLinHash* f, int n, uint64_t key, uint64_t tid) {
  srand(0);
  for (int i = 0; i < n; i++) {
    // uint64_t k = key * (rand() % 2) + rand() % 100;
    uint64_t k = key + i;
    insertWithMsg(f, k, k);
  }
  return 0;
}

/**
 * @brief test multiThread
 * @param f the db engine
 * @return 
 */
int TestMultiThread(pmLinHash* f) {
  f->clear();
  f->showKV();

  thread threads[5];
  for (size_t i = 0; i < 5; i++) {
    threads[i] = thread(justInsertNWithMsg, f, 100, i + 1, i);
  }

  for (auto& i : threads) {
    i.join();
  }
  f->showKV();
  // f->showBitMap();
  return 0;
}

/**
 * @brief use a series of assert to test
 * @param f the db engine
 * @return 
 */
int AssertTEST(pmLinHash* f) {
  // the allowable size is between 300000 and 400000,
  // otherwise trigger memory size limit
  const int N = 300000;
  const int ASSERT_STEP = 5000;

  printf("=========Assert Test==========\n");
  printf("TEST %d KVs\n", N);
  printf("Check Empty Env...\n");
  for (uint64_t i = 0; i < N; i++) {
    uint64_t value;
    assert(f->search(i, value) == -1);
    assert(f->update(i, value) == -1);
    assert(f->remove(i) == -1);
  }
  // insert
  printf("Check Insert&Search...\n");
  for (uint64_t i = 0; i < N; i++) {
    assert(f->insert(i, i) != -1);
    assert(f->insert(i, i) == -1);
    // search history key&value
    for (uint64_t j = 0; j <= i; j += ASSERT_STEP) {
      uint64_t value = N;
      assert(f->search(j, value) != -1);
      assert(j == value);
    }
  }
  // update
  printf("Check Update&Search...\n");
  for (uint64_t i = 0; i < N; i++) {
    assert(f->update(i, i + 1) != -1);
    // search history key&value
    for (uint64_t j = 0; j <= i; j += ASSERT_STEP) {
      uint64_t value = N;
      assert(f->search(j, value) != -1);
      assert(value == j + 1);
      // can't insert dupKey
      assert(f->insert(j, j) == -1);
    }
  }
  // remove
  printf("Check Remove&Search...\n");
  for (uint64_t i = 0; i < N; i++) {
    assert(f->remove(i) != -1);
    uint64_t value;
    for (uint64_t j = 0; j <= i; j += ASSERT_STEP) {
      assert(f->remove(j) == -1);
      assert(f->search(j, value) == -1);
      assert(f->update(j, j + 1) == -1);
    }
    for (uint64_t j = i + 1; j < N; j += ASSERT_STEP) {
      uint64_t value;
      assert(f->search(j, value) != -1);
    }
  }
  // clear
  printf("Check Clear&Search...\n");
  f->clear();
  for (int i = 0; i < N; i++) {
    uint64_t value;
    assert(f->search(i, value) == -1);
  }

  printf("=========Assert OK============\n");
  return 0;
}

/**
 * @brief YCSB benchmark
 * @param f the db engine
 * @param n_thread the number of thread to run
 * @param path_prefix the file path prefix related to the root path of the project
 * @return 
 */
int BenchmarkYCSB(pmLinHash* f, uint64_t n_thread, string path_prefix) {
  printf("=========YCSB Benchmark=======\n");
  printf("注:时间基本为纯Insert/Search时间,不包括读文件\tn_thread=%zu\n",
         n_thread);
  f->clear();
  loadYCSBBenchmark(path_prefix + "./benchmark/10w-rw-0-100-load.txt", f);
  runYCSBBenchmark(path_prefix + "./benchmark/10w-rw-0-100-load.txt", f,
                   n_thread);
  f->clear();
  loadYCSBBenchmark(path_prefix + "./benchmark/10w-rw-25-75-load.txt", f);
  runYCSBBenchmark(path_prefix + "./benchmark/10w-rw-25-75-load.txt", f,
                   n_thread);
  f->clear();
  loadYCSBBenchmark(path_prefix + "./benchmark/10w-rw-50-50-load.txt", f);
  runYCSBBenchmark(path_prefix + "./benchmark/10w-rw-50-50-load.txt", f,
                   n_thread);
  f->clear();
  loadYCSBBenchmark(path_prefix + "./benchmark/10w-rw-75-25-load.txt", f);
  runYCSBBenchmark(path_prefix + "./benchmark/10w-rw-75-25-load.txt", f,
                   n_thread);
  f->clear();
  loadYCSBBenchmark(path_prefix + "./benchmark/10w-rw-100-0-load.txt", f);
  runYCSBBenchmark(path_prefix + "./benchmark/10w-rw-100-0-load.txt", f,
                   n_thread);
  f->clear();
  printf("==========Benchmark OK=========\n");
  return 0;
}

/**
 * @brief the is worker thread to write and read from the pmLinearHash
 * @param buf the entries waiting to write or read
 * @param f the db engine
 * @param tid thread id
 * @param n_thread number of thread
 * @return 
 */
int thread_routine(vector<std::pair<char, uint64_t>>& buf,
                   pmLinHash* f,
                   uint64_t tid,
                   uint64_t n_thread) {
  auto size = buf.size();
  for (size_t i = tid; i < size; i += n_thread) {
    auto& key = buf[i];
    if (key.first == INSERT) {
      f->insert(key.second, key.second);
    } else {
      uint64_t value;
      assert(f->search(key.second, value) != -1);
      assert(value == key.second);
    }
  }
  return 0;
}
