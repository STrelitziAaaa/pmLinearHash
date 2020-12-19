#include "pmLinHash.h"
#include "util.h"

thread_local pm_err pmError;


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
#define DEBUG
#ifdef DEBUG
    initMappedMem();
#endif
  } else {
    // init
    initMappedMem();
  }
}

/**
 * PMLHash::~PMLHash
 *
 * unmap and close the data file
 */
pmLinHash::~pmLinHash() {
  pmem_unmap(start_addr, FILE_SIZE);
}
/**
 * PMLHash
 *
 * split the hash table indexed by the meta->next
 * update the metadata
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
  meta->updateNextPtr();
}
/**
 * PMLHash
 *
 * @param  {uint64_t} key     : key
 * @param  {size_t} hash_size : the N in hash func: idx = hash % N
 * @return {uint64_t}         : index of hash table array
 *
 * need to hash the key with proper hash function first
 * then calculate the index by N module
 */
uint64_t pmLinHash::hashFunc(const uint64_t& key, const size_t& level) {
  return key % uint64_t(N_LEVEL(level));
}

/**
 * PMLHash
 *
 * @param  {uint64_t} offset : the file address offset of the overflow hash
 * table to the start of the whole file
 * @return {pm_table*}       : the virtual address of new overflow hash table
 *
 * @fixed: just use overflow_addr + meta.overflow_num to compute the new addr
 */
pm_table* pmLinHash::newOverflowTable() {
  uint64_t offset = bitmap->findFirstAvailable();
  if (offset == BITMAP_SIZE) {
    // after throw this msg, the db may still can insert other kv, but we still
    // view it as full
    // we just throw error,instead of returning nullptr
    pmError.set(OfMemoryLimitErr);
    throw Error("newOverflowTable: memory size limit");
  }
  bitmap->set(offset);

  pm_table* table = (pm_table*)((pm_table*)overflow_addr + offset);
  table->init();
  meta->overflow_num++;
  return table;
}

pm_table* pmLinHash::newOverflowTable(pm_table* pre) {
  pm_table* table = newOverflowTable();
  pre->next_offset = getIdxFromOfTable(table);
  return table;
}

// if this table have previousTable, you should set previousTable.next_offset
int pmLinHash::freeOverflowTable(uint64_t idx) {
  bitmap->reset(idx);
  meta->overflow_num--;
  pmem_persist(&(meta->overflow_num), sizeof(uint64_t));
  return 0;
}

int pmLinHash::freeOverflowTable(pm_table* t) {
  return freeOverflowTable(getIdxFromOfTable(t));
}

// it will use meta.size to address , and init table
pm_table* pmLinHash::newNormalTable() {
  if (meta->size >= NORMAL_TAB_SIZE) {
    pmError.set(NmMemoryLimitErr);
    throw Error("newNormalTable: memory size limit");
  }
  pm_table* table = (pm_table*)(table_arr + meta->size);
  table->init();
  meta->size++;
  return table;
}

// NmTable : Normal Table, which is not a overflow table
pm_table* pmLinHash::getNmTableFromIdx(uint64_t idx) {
  if (idx == NEXT_IS_NONE) {
    return nullptr;
  }
  return (pm_table*)((pm_table*)table_arr + idx);
}
// OfTable : Overflow Table
pm_table* pmLinHash::getOfTableFromIdx(uint64_t idx) {
  if (idx == NEXT_IS_NONE) {
    return nullptr;
  }
  return (pm_table*)((pm_table*)overflow_addr + idx);
}

uint64_t pmLinHash::getIdxFromTable(pm_table* start, pm_table* t) {
  return (t - start);
}

// get index from overflow table
uint64_t pmLinHash::getIdxFromOfTable(pm_table* t) {
  return getIdxFromTable((pm_table*)overflow_addr, t);
}

uint64_t pmLinHash::getIdxFromNmTable(pm_table* t) {
  return getIdxFromTable((pm_table*)start_addr, t);
}

// deprecated: it's used only once and we can searchEntry directly instead.
bool pmLinHash::checkDupKey(const uint64_t& key) {
  entry* e = searchEntry(key);
  if (e != nullptr) {
    return true;
  }
  return false;
}

uint64_t pmLinHash::splitOldIdx() {
  return meta->next;
}
uint64_t pmLinHash::splitNewIdx() {
  return meta->next + N_LEVEL(meta->level);
}

uint64_t pmLinHash::getRealHashIdx(const uint64_t& key) {
  uint64_t idx = hashFunc(key, meta->level);
  if (idx < meta->next) {
    idx = hashFunc(key, meta->level + 1);
  }
  return idx;
}

// append auto overflow
// return 1 if needToSplit , it will automatically allocate new overflowTable
int pmLinHash::appendAutoOf(pm_table* startTable, const entry& kv) {
  pm_table* cur = startTable;
  int needTosplit = 0;
  while (cur->append(kv.key, kv.value) == -1) {
    // check if we can go through
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
int pmLinHash::insertAutoOf(pm_table* startTable, const entry& kv, uint64_t pos) {
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

// update fill_num and next_offset
int pmLinHash::updateAfterSplit(pm_table* startTable, uint64_t fill_num) {
  // todo: wait to optimizing, the code below is ugly
  // first count original number of overflow tables
  int n = cntTablesSince(startTable);
  int n_fill = fill_num / (TABLE_SIZE + 1);
  // we shouldn't go to the last overflow table
  // in case of the last overflow table becoming empty
  for (int i = 0; i < n_fill; i++) {
    // you must set it full because newTable.fill_num = 0
    startTable->fill_num = TABLE_SIZE;
    startTable = getOfTableFromIdx(startTable->next_offset);
  }
  startTable->fill_num = fill_num % (TABLE_SIZE + 1);
  pm_table* lastTable = startTable;

  for (int i = n_fill; i < n; i++) {
    uintptr_t idx = startTable->next_offset;
    startTable = getOfTableFromIdx(startTable->next_offset);
    freeOverflowTable(idx);
  }
  lastTable->next_offset = NEXT_IS_NONE;
  return 0;
}

// never include startTable itself
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
 * PMLHash
 *
 * @param  {uint64_t} key   : inserted key
 * @param  {uint64_t} value : inserted value
 * @return {int}            : success: 0. fail: -1
 *
 * insert the new kv pair in the hash
 *
 * always insert the entry in the first empty slot
 *
 * if the hash table is full then split is triggered
 */
int pmLinHash::insert(const uint64_t& key, const uint64_t& value) {
  std::unique_lock wr_lock(rwMutx);

  if (searchEntry(key) != nullptr) {
    pmError.set(DupKeyErr);
    return -1;
  }

  try {
    if (appendAutoOf(getNmTableFromIdx(getRealHashIdx(key)),
                     entry::makeEntry(key, value))) {
      split();
    }
  } catch (Error& e) {
    pmError.perror("insert");
    return -1;
  }
  pmem_persist(start_addr, FILE_SIZE);
  return 0;
}

/**
 * PMLHash
 *
 * @param  {uint64_t} key   : the searched key
 * @param  {uint64_t} value : return value if found , else VALUE_NOT_FOUND
 * @return {int}            : 0 found, -1 not found
 *
 * search the target entry and return the value
 */
int pmLinHash::search(const uint64_t& key, uint64_t& value) {
  std::shared_lock rd_lock(rwMutx);

  entry* e = searchEntry(key);
  if (e == nullptr) {
    return -1;
  }
  value = e->value;
  return 0;
}

/**
 * PMLHash
 *
 * @param  {uint64_t} key : target key
 * @return {int}          : success: 0. fail: -1
 *
 * remove the target entry, move entries after forward
 * if the overflow table is empty, remove it from hash
 */
int pmLinHash::remove(const uint64_t& key) {
  std::unique_lock wr_lock(rwMutx);
  // you should find its previous table
  pm_table* pre = nullptr;
  uint64_t pos;
  pm_table* t = searchEnteyWithPage(key, &pos, &pre);
  if (t == nullptr) {
    pmError.set(NotFoundErr);
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
    pre->next_offset = NEXT_IS_NONE;
  }
  pmem_persist(start_addr, FILE_SIZE);

  return 0;
}

/**
 * PMLHash
 *
 * @param  {uint64_t} key   : target key
 * @param  {uint64_t} value : new value
 * @return {int}            : success: 0. fail: -1
 *
 * update an existing entry
 */
int pmLinHash::update(const uint64_t& key, const uint64_t& value) {
  std::unique_lock wr_lock(rwMutx);

  entry* e = searchEntry(key);
  if (e == nullptr) {
    return -1;
  }

  e->value = value;
  pmem_persist(e, sizeof(entry));
  return 0;
}

// it will init mapped memory, call it after setting startAddr
int pmLinHash::initMappedMem() {
  bzero(start_addr, FILE_SIZE);
  meta->init();
  bitmap->init();
  pm_table::initArray(table_arr, N_0);
  return 0;
}

int pmLinHash::recoverMappedMen() {
  // you don't need do anything!
  return 0;
}

// remove all ; it is not concurency-safe
int pmLinHash::clear() {
  initMappedMem();
  pmem_persist(start_addr, FILE_SIZE);
  return 0;
}

// -----------------------------------

int pmLinHash::showConfig() {
  printf("=========PMLHash Config=======\n");
  printf("- start_addr:%p\n", start_addr);
  printf("- overflow_addr:%p\n", overflow_addr);
  printf("- meta_addr:%p\n", meta);
  printf("  - size:%zu level:%zu next:%zu overflow_num:%zu\n", meta->size,
         meta->level, meta->next, meta->overflow_num);
  printf("- table_addr:%p\n", table_arr);
  showKV("  - ");
  printf("- bitmap_addr:%p\n", bitmap);
  printf("=========Init Config=======\n");
  printf("TABLE_SIZE:%d\n", TABLE_SIZE);
  printf("HASH_SIZE:%d\n", HASH_SIZE);
  printf("FILE_SIZE:%d\n", FILE_SIZE);
  printf("BITMAP_SIZE:%zu\n", BITMAP_SIZE);
  printf("OVERFLOW_TAB_SIZE:%zu\n", OVERFLOW_TABL_SIZE);
  printf("NORMAL_TAB_SIZE:%zu\n", NORMAL_TAB_SIZE);
  printf("N_0:%d\n", N_0);
  printf("[Support at least %zu KVs <not include overflowTable>]\n",
         TABLE_SIZE * NORMAL_TAB_SIZE);
  printf("===========Config OK==========\n");
  return 0;
}

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

int pmLinHash::showBitMap() {
  printf("%s\n", bitmap->to_string().c_str());
  return 0;
}
