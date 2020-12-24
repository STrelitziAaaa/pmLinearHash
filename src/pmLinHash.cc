#include "pmLinHash.h"
#include <errno.h>
#include <map>
#include "pmUtil.h"

/**
 * @brief construct function of pmLinHash
 * @param file_path
 */
pmLinHash::pmLinHash(const char* file_path) {
  // you must check whether file exists first
  // otherwise pmem_map_file with PMEM_FILE_CREATE|PMEM_FILE_EXCL will fail
  // directly if file exists instead of setting errno
  // so first stat() will help to avoid invoking pmem_map_file twice
  bool fileExists = chkAndCrtFile(file_path);
  size_t mapped_len;
  int is_pmem;
  uint8_t* f = (uint8_t*)pmem_map_file(file_path, FILE_SIZE, PMEM_FILE_CREATE,
                                       0666, &mapped_len, &is_pmem);
  if (mapped_len != FILE_SIZE || !f) {
    perror("pmem_map_file");
    exit(EXIT_FAILURE);
  }
  printf("mapped_len:%zu , %f mb, ptr:%p\n", mapped_len,
         double(mapped_len) / (1024 * 1024), f);
  start_addr = (void*)f;
  overflow_addr = (void*)(f + FILE_SIZE / 2);
  meta = (metadata*)(f);
  bitmap = (bitmap_st*)(meta + 1);
  table_arr = (pm_table*)(bitmap + 1);

  if (fileExists) {
// recover
#ifdef PMDEBUG
    printf("debug: init()\n");
    initMappedMem();
#else
    printf("debug: recover()\n");
    recoverMappedMen();
#endif
  } else {
    // init
    initMappedMem();
  }
}

/**
 * @brief unmap and close the data file
 */
pmLinHash::~pmLinHash() {
  pmem_unmap(start_addr, FILE_SIZE);
}

/**
 * @brief split the hash table indexed by the meta->next and update metadata
 * @return (void)
 */
void pmLinHash::split() {
  pm_table* oldTable = getNmTableFromIdx(meta->next);
  pm_table* curTable = oldTable;
  pm_table* newTable = newNormalTable();
  map<uint64_t, uint64_t> fill_num;
  fill_num[splitOldIdx()] = 0;
  fill_num[splitNewIdx()] = 0;
  while (1) {
    for (size_t i = 0; i < curTable->fill_num; i++) {
      entry& kv = curTable->kv_arr[i];
      uint64_t idx = hashFunc(kv.key, meta->level + 1);
      insertAutoOf(getNmTableFromIdx(idx), kv, fill_num[idx]);
      fill_num[idx]++;
    }
    // 可能的问题: 判断的时候不等于,但是马上被其他线程修改了?
    uint64_t a = curTable->next_offset;
    if (curTable->next_offset != NEXT_IS_NONE) {
      // * note: this assert is essential if used in multiThread
      // * you will find each bucket with rw_mutex is difficult
      // * to implement
      assert(curTable->fill_num == TABLE_SIZE);
      // go to its next overflow table
      curTable = getOfTableFromIdx(curTable->next_offset);
    } else {
      break;
    }
  }

  updateAfterSplit(oldTable, fill_num[splitOldIdx()]);
  updateAfterSplit(newTable, fill_num[splitNewIdx()]);
  // update the next of metadata
  meta->addNextPtr(1);
}

/**
 * @brief compute the target index
 * @param key
 * @param level the level of the linear hash, defined in the meta->level
 * @return index of hash table array
 */
uint64_t pmLinHash::hashFunc(const uint64_t& key, const size_t& level) {
  return key % uint64_t(N_LEVEL(level));
}

/**
 * @brief require and init new overflow table
 * @param pre automatically set pre->next = the new allocated table
 * @return the new allocated table
 */
pm_table* pmLinHash::newOverflowTable(pm_table* pre) {
  uint64_t offset = bitmap->findFirstAvailable();
  if (offset == BITMAP_SIZE) {
    // after throw this msg, the db may still can insert other kv, but we still
    // view it as full
    // we just throw error,instead of returning nullptr
    pmError_tls.OfmemoryLimit();
    throw "newOverflowTable: memory size limit";
  }
  bitmap->set(offset);
  pm_table* table = (pm_table*)((pm_table*)overflow_addr + offset);
  table->init();
  meta->addOverflow(1);

  if (pre != nullptr) {
    pre->setNextOffset(offset);
  }
  return table;
}

/**
 * @brief pmOK: free the table; it never change the next of the previous table
 * @param idx the offset regarding the first overflow table
 * @return nothing
 */
int pmLinHash::freeOverflowTable(uint64_t idx) {
  bitmap->reset(idx);
  meta->addOverflow(-1);
  return 0;
}

/**
 * @brief pmOK: free the table
 * @param t the table addr
 * @return nothing
 */
int pmLinHash::freeOverflowTable(pm_table* t) {
  return freeOverflowTable(getIdxFromOfTable(t));
}

/**
 * @brief pmOK: require and init new normal table
 * @return the allocated table
 */
pm_table* pmLinHash::newNormalTable() {
  if (meta->size >= NORMAL_TAB_SIZE) {
    pmError_tls.NmmemoryLimit();
    throw "newNormalTable: memory size limit";
  }
  pm_table* table = (pm_table*)(table_arr + meta->size);
  table->init();
  meta->addSize(1);
  return table;
}

/**
 * @brief  map the idx of the normal table to ptr
 * @param idx offset
 * @return table addr
 */
pm_table* pmLinHash::getNmTableFromIdx(uint64_t idx) {
  if (idx == NEXT_IS_NONE) {
    return nullptr;
  }
  return (pm_table*)((pm_table*)table_arr + idx);
}

/**
 * @brief map the idx of the overflow table to ptr
 * @param idx offset
 * @return table addr
 */
pm_table* pmLinHash::getOfTableFromIdx(uint64_t idx) {
  if (idx == NEXT_IS_NONE) {
    return nullptr;
  }
  return (pm_table*)((pm_table*)overflow_addr + idx);
}

/**
 * @brief compute offset
 * @param start
 * @param t
 * @return t-start
 */
uint64_t pmLinHash::getIdxFromTable(pm_table* start, pm_table* t) {
  return (t - start);
}

/**
 * @brief get offset(index) from overflow table
 * @param t
 * @return
 */
uint64_t pmLinHash::getIdxFromOfTable(pm_table* t) {
  return getIdxFromTable((pm_table*)overflow_addr, t);
}

/**
 * @brief get offset(index) from normal table
 * @param t
 * @return
 */
uint64_t pmLinHash::getIdxFromNmTable(pm_table* t) {
  return getIdxFromTable((pm_table*)start_addr, t);
}

/**
 * @brief deprecated: we can use searchEntry instead.
 * @param key
 * @return
 */
bool pmLinHash::checkDupKey(const uint64_t& key) {
  entry* e = searchEntry(key);
  if (e != nullptr) {
    return true;
  }
  return false;
}

/**
 * @brief
 * @return return the old idx of the bucket
 */
uint64_t pmLinHash::splitOldIdx() {
  return meta->next;
}
/**
 * @brief
 * @return return the new idx of the bucket
 */
uint64_t pmLinHash::splitNewIdx() {
  return meta->next + N_LEVEL(meta->level);
}

/**
 * @brief compute the real hash idx,it will use meta->level+1
 * @param key
 * @return
 */
uint64_t pmLinHash::getRealHashIdx(const uint64_t& key) {
  uint64_t idx = hashFunc(key, meta->level);
  if (idx < meta->next) {
    idx = hashFunc(key, meta->level + 1);
  }
  return idx;
}

/**
 * @brief append an entry to a bucket, it will automatically newOverflowTable
 * @param startTable the table addr of the first normal table of a bucket
 * @param kv entry wait to append
 * @return return 1 if needToSplit
 */
int pmLinHash::appendAutoOf(pm_table* startTable, const entry& kv) {
  pm_table* cur = startTable;
  int needTosplit = 0;
  while (cur->append(kv.key, kv.value) == -1) {
    // check if we can go to next
    if (cur->next_offset == NEXT_IS_NONE) {
      // cur->next_offset = meta->overflow_num;
      cur = newOverflowTable(cur);
      needTosplit = 1;
    } else {
      cur = getOfTableFromIdx(cur->next_offset);
    }
  }
  return needTosplit;
}

// insert auto overflow
// it will allocate new overflow table OR just go through
// it never change fill_num
/**
 * @brief pmOK: insert an entry to a bucket, it will automatically
 * newOverflowTable
 * @param startTable the table addr of the first normal table of a bucket
 * @param kv entry wait to insert
 * @param pos position wait to insert in
 * @return nothing
 */
int pmLinHash::insertAutoOf(pm_table* startTable,
                            const entry& kv,
                            uint64_t pos) {
  uint64_t n = pos / TABLE_SIZE;
  while (n > 0) {
    if (startTable->next_offset != NEXT_IS_NONE) {
      startTable = getOfTableFromIdx(startTable->next_offset);
    } else {
      startTable = newOverflowTable(startTable);
    }
    n--;
  }
  startTable->insert(kv.key, kv.value, pos % TABLE_SIZE);
  return 0;
}

/**
 * @brief update related table->fill_num and table->next_offset
 * @param startTable the first normal table of a bucket
 * @param fill_num the total number of entries of a bucket
 * @return nothing
 */
int pmLinHash::updateAfterSplit(pm_table* startTable, uint64_t fill_num) {
  // todo: wait to optimizing, the code below is ugly
  // first count original number of overflow tables
  int n = cntTablesSince(startTable);
  int n_fill = fill_num / (TABLE_SIZE + 1);
  // we shouldn't go to the last overflow table
  // in case of the last overflow table becoming empty
  pm_table* t = startTable;
  for (int i = 0; i < n_fill; i++) {
    // you must set it full because newTable.fill_num = 0
    t->setFillNum(TABLE_SIZE);
    t = getOfTableFromIdx(t->next_offset);
  }
  t->setFillNum(fill_num % (TABLE_SIZE + 1));
  pm_table* lastTable = t;

  // release tables that is not needed
  for (int i = n_fill; i < n; i++) {
    uintptr_t idx = t->next_offset;
    t = getOfTableFromIdx(idx);
    freeOverflowTable(idx);
  }
  lastTable->setNextOffset(NEXT_IS_NONE);
  return 0;
}

/**
 * @brief count the number of tables after the table,it never includes itself
 * @param startTable
 * @return return count
 */
int pmLinHash::cntTablesSince(pm_table* startTable) {
  int cnt = 0;
  while (startTable->next_offset != NEXT_IS_NONE) {
    startTable = getOfTableFromIdx(startTable->next_offset);
    cnt++;
  }
  return cnt;
}

/**
 * @brief searchEntry with key
 * @param key
 * @return return nullptr if not found
 */
entry* pmLinHash::searchEntry(const uint64_t& key) {
  uint64_t pos;
  pm_table* t = searchEnteyWithPage(key, &pos);
  if (t == nullptr) {
    return nullptr;
  }
  return t->index(pos);
}

/**
 * @brief this will just be used in `remove()`
 * @param key
 * @param pos the position of the key in the table
 * @param previousTable the previous table of the return table
 * @return the table of the key
 */
pm_table* pmLinHash::searchEnteyWithPage(const uint64_t& key,
                                         uint64_t* pos,
                                         pm_table** previousTable) {
  uint64_t idx = getRealHashIdx(key);
  pm_table* t = getNmTableFromIdx(idx);
  pm_table* pre = nullptr;
  while (1) {
    int pos_ = t->pos(key);
    if (pos_ >= 0) {
      if (pos != nullptr) {
        *pos = uint64_t(pos_);
      }
      if (previousTable != nullptr) {
        *previousTable = pre;
      }
      return t;
    }
    if (t->next_offset == NEXT_IS_NONE) {
      break;
    }
    pre = t;
    t = getOfTableFromIdx(t->next_offset);
  }
  return nullptr;
}

/**
 * @brief insert entry
 * @param key
 * @param value
 * @return return -1 if full, 0 if success
 */
int pmLinHash::insert(const uint64_t& key, const uint64_t& value) {
  std::unique_lock wr_lock(rwMutx);

  if (searchEntry(key) != nullptr) {
    pmError_tls.dupkey();
    return -1;
  }

  try {
    if (appendAutoOf(getNmTableFromIdx(getRealHashIdx(key)),
                     entry::makeEntry(key, value))) {
      split();
    }
  } catch (const char* msg) {
    printf("%s\n", msg);
    pmError_tls.perror("insert");
    exit(EXIT_FAILURE);
    return -1;
  }
  // pmem_persist(start_addr, FILE_SIZE);

  pmError_tls.success();
  return 0;
}

/**
 * @brief deprecated: use pmLinHash::search(const uint64_t& key,
 * uint64_t* value) instead; It is not recommended, cuz if call search(a,b) ,u
 * don't know b will be rewritten. but if call search(a,&b), it's obvious.
 * @param key
 * @param value the value will be rewritten
 * @return return -1 if not found ; 0 if success
 */
int pmLinHash::search(const uint64_t& key, uint64_t& value) {
  std::shared_lock rd_lock(rwMutx);

  entry* e = searchEntry(key);
  if (e == nullptr) {
    pmError_tls.notfound();
    return -1;
  }
  value = e->value;

  pmError_tls.success();
  return 0;
}

/**
 * @brief search
 * @param key
 * @param value the value will be rewritten
 * @return return -1 if not found ; 0 if success
 */
int pmLinHash::search(const uint64_t& key, uint64_t* value) {
  std::shared_lock rd_lock(rwMutx);

  entry* e = searchEntry(key);
  if (e == nullptr) {
    pmError_tls.notfound();
    return -1;
  }
  *value = e->value;

  pmError_tls.success();
  return 0;
}

/**
 * @brief remove
 * @param key
 * @return return -1 if not found, 0 if success
 */
int pmLinHash::remove(const uint64_t& key) {
  std::unique_lock wr_lock(rwMutx);

  // you should find its previous table
  pm_table* pre = nullptr;
  uint64_t pos;
  pm_table* t = searchEnteyWithPage(key, &pos, &pre);
  if (t == nullptr) {
    pmError_tls.notfound();
    return -1;
  }

  entry* lastKV = t->index(t->fill_num - 1);
  t->insert(lastKV->key, lastKV->value, pos);

  while (t->next_offset != NEXT_IS_NONE) {
    pre = t;
    t = getOfTableFromIdx(t->next_offset);
    entry* lastKV = t->index(t->fill_num - 1);
    assert(pre->fill_num == TABLE_SIZE);
    pre->insert(lastKV->key, lastKV->value, TABLE_SIZE - 1);
  }

  if (--(t->fill_num) == 0 && pre != nullptr) {
    freeOverflowTable(t);
    pre->setNextOffset(NEXT_IS_NONE);
  }

  pmError_tls.success();
  return 0;
}

/**
 * @brief update
 * @param key
 * @param value
 * @return -1 if not found , 0 if success
 */
int pmLinHash::update(const uint64_t& key, const uint64_t& value) {
  std::unique_lock wr_lock(rwMutx);

  entry* e = searchEntry(key);
  if (e == nullptr) {
    pmError_tls.notfound();
    return -1;
  }

  e->setValue(value);

  pmError_tls.success();
  return 0;
}

/**
 * @brief pmOK: init mapped file(pmem), call it after set start_addr
 * @return nothing
 */
int pmLinHash::initMappedMem() {
  bzero(start_addr, FILE_SIZE);
  pmem_persist(start_addr, FILE_SIZE);

  meta->init();
  bitmap->init();
  pm_table::initArray(table_arr, N_0);
  return 0;
}

/**
 * @brief recover from mapped file(pmem)
 * @return nothing
 */
int pmLinHash::recoverMappedMen() {
  // you don't need do anything!
  return 0;
}

/**
 * @brief remove all entries, it is never concurency-safe
 * @return nothing
 */
int pmLinHash::clear() {
  initMappedMem();
  return 0;
}

// -----------------------------------

/**
 * @brief print config, used in test
 * @return
 */
int pmLinHash::showConfig() {
  printf("=========PMLHash Config=========\n");
  printf("- start_addr:%p\n", start_addr);
  printf("- overflow_addr:%p\n", overflow_addr);
  printf("- meta_addr:%p\n", meta);
  printf("  - size:%zu level:%zu next:%zu overflow_num:%zu\n", meta->size,
         meta->level, meta->next, meta->overflow_num);
  printf("- table_addr:%p\n", table_arr);
  // showKV("  - ");
  printf("- bitmap_addr:%p\n", bitmap);
  printf("=========Init Config=========\n");
  printf("TABLE_SIZE:%d\n", TABLE_SIZE);
  printf("HASH_SIZE:%d\n", HASH_SIZE);
  printf("FILE_SIZE:%d\n", FILE_SIZE);
  printf("BITMAP_SIZE:%zu\n", BITMAP_SIZE);
  printf("OVERFLOW_TAB_SIZE:%zu\n", OVERFLOW_TABL_SIZE);
  printf("NORMAL_TAB_SIZE:%zu\n", NORMAL_TAB_SIZE);
  printf("N_0:%d\n", N_0);
  printf("[Support at least %zu KVs <not include overflowTable>]\n",
         TABLE_SIZE * NORMAL_TAB_SIZE);
  printf("===========Config OK============\n");
  return 0;
}

/**
 * @brief print all entries, used in test
 * @param prefix the prefix string in front of every bucket
 * @return
 */
int pmLinHash::showKV(const char* prefix) {
  printf("===========Table View=========\n");
  for (size_t i = 0; i < meta->size; i++) {
    pm_table* t = getNmTableFromIdx(i);
    printf("%sTable:%lu ", prefix, i);
    while (1) {
      if (t->fill_num == 0) {
        printf("empty");
      }
      for (size_t j = 0; j < t->fill_num; j++) {
        printf("%zu,%zu|", t->kv_arr[j].key, t->kv_arr[j].value);
      }
      if (t->next_offset != NEXT_IS_NONE) {
        t = getOfTableFromIdx(t->next_offset);
        printf(" -o- ");
      } else {
        break;
      }
    }
    printf("\n");
  }
  printf("===========Table OK===========\n");
  return 0;
}

/**
 * @brief print bitmap, used in test
 * @return
 */
int pmLinHash::showBitMap() {
  printf("%s\n", bitmap->to_string().c_str());
  return 0;
}