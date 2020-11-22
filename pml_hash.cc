#include "pml_hash.h"
#include <assert.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <map>
#include "util.h"

/**
 * PMLHash::PMLHash
 *
 * @param  {char*} file_path : the file path of data file
 * if the data file exist, open it and recover the hash
 * if the data file does not exist, create it and initial the hash
 */
PMLHash::PMLHash(const char* file_path) {
  // you must check whether file exists first
  // because pmem_map_file with PMEM_FILE_CREATE|PMEM_FILE_EXCL will fail
  // directly if file exists instead of setting errno
  // so first stat() will help to avoid invoking pmem_map_file twice
  bool fileIsExist = chkAndCrtFile(file_path);

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

  if (fileIsExist) {
// recover
#define DEBUG
#ifdef DEBUG
    bzero(f, FILE_SIZE);
    meta->init();
    bitmap->init();
    pm_table::initArray(table_arr, N_0);
#endif
  } else {
    // init
    bzero(f, FILE_SIZE);
    meta->init();
    bitmap->init();
    pm_table::initArray(table_arr, N_0);
  }
}

/**
 * PMLHash::~PMLHash
 *
 * unmap and close the data file
 */
PMLHash::~PMLHash() {
  pmem_unmap(start_addr, FILE_SIZE);
}
/**
 * PMLHash
 *
 * split the hash table indexed by the meta->next
 * update the metadata
 */
void PMLHash::split() {
  pm_table* oldTable = getNmTableFromIdx(meta->next);
  pm_table* curTable = oldTable;
  pm_table* newTable = newNormalTable();
  map<uint64_t, uint64_t> fill_num;
  fill_num[splitOldIdx()] = 0;
  fill_num[splitNewIdx()] = 0;
  while (1) {
    for (int i = 0; i < curTable->fill_num; i++) {
      entry& kv = curTable->kv_arr[i];
      uint64_t idx = hashFunc(kv.key, meta->level + 1);
      insertAutoOf(getNmTableFromIdx(idx), kv, fill_num[idx]);
      fill_num[idx]++;
    }
    if (curTable->next_offset != NEXT_IS_NONE) {
      // it has overflow table
      assert(curTable->fill_num == TABLE_SIZE);
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
uint64_t PMLHash::hashFunc(const uint64_t& key, const size_t& level) {
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
pm_table* PMLHash::newOverflowTable() {
  uint64_t offset = bitmap->findFirstAvailable();
  bitmap->set(offset);

  pm_table* table = (pm_table*)((pm_table*)overflow_addr + offset);
  table->init();
  meta->overflow_num++;
  return table;
}

pm_table* PMLHash::newOverflowTable(pm_table* pre) {
  uint64_t offset = bitmap->findFirstAvailable();
  bitmap->set(offset);
  pre->next_offset = offset;

  pm_table* table = (pm_table*)((pm_table*)overflow_addr + offset);
  table->init();
  meta->overflow_num++;
  return table;
}

// if this table have previousTable, you should set previousTable.next_offset
int PMLHash::freeOverflowTable(uint64_t idx) {
  bitmap->reset(idx);
  meta->overflow_num--;
}

int PMLHash::freeOverflowTable(pm_table* t) {
  return freeOverflowTable(getIdxFromOfTable(t));
}

// it will use meta.size to address , and init table
pm_table* PMLHash::newNormalTable() {
  pm_table* table = (pm_table*)(table_arr + meta->size);
  table->init();
  meta->size++;
  return table;
}

// NmTable : Normal Table, which is not a overflow table
pm_table* PMLHash::getNmTableFromIdx(uint64_t idx) {
  return (pm_table*)((pm_table*)table_arr + idx);
}
// OfTable : Overflow Table
pm_table* PMLHash::getOfTableFromIdx(uint64_t idx) {
  return (pm_table*)((pm_table*)overflow_addr + idx);
}

uint64_t PMLHash::getIdxFromTable(pm_table* start, pm_table* t) {
  return (t - start);
}

// get index from overflow table
uint64_t PMLHash::getIdxFromOfTable(pm_table* t) {
  return getIdxFromTable((pm_table*)overflow_addr, t);
}

bool PMLHash::checkDupKey(const uint64_t& key) {
  entry* e = searchEntry(key);
  if (e != nullptr) {
    return true;
  }
  return false;
}

uint64_t PMLHash::splitOldIdx() {
  return meta->next;
}
uint64_t PMLHash::splitNewIdx() {
  return meta->next + N_LEVEL(meta->level);
}

uint64_t PMLHash::getRealHashIdx(const uint64_t& key) {
  uint64_t idx = hashFunc(key, meta->level);
  if (idx < meta->next) {
    idx = hashFunc(key, meta->level + 1);
  }
  return idx;
}

// append auto overflow
// return 1 if needToSplit , it will automatically allocate overflowTable
int PMLHash::appendAutoOf(pm_table* startTable, const entry& kv) {
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
int PMLHash::insertAutoOf(pm_table* startTable, const entry& kv, uint64_t pos) {
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
}

// update fill_num and next_offset
int PMLHash::updateAfterSplit(pm_table* startTable, uint64_t fill_num) {
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
}

// never include startTable itself
int PMLHash::cntTablesSince(pm_table* startTable) {
  int cnt = 0;
  while (startTable->next_offset != NEXT_IS_NONE) {
    assert(startTable->fill_num == TABLE_SIZE);
    startTable = getOfTableFromIdx(startTable->next_offset);
    cnt++;
  }
  return cnt;
}

// return nullptr if not found
entry* PMLHash::searchEntry(const uint64_t& key) {
  uint64_t idx = getRealHashIdx(key);

  pm_table* t = getNmTableFromIdx(idx);
  while (1) {
    int pos = t->pos(key);
    if (pos >= 0) {
      return &(t->index(pos));
    }
    if (t->next_offset == NEXT_IS_NONE) {
      break;
    }
    t = getOfTableFromIdx(t->next_offset);
  }
  return nullptr;
}

/**
 * @brief this will just be used in `remove()`
 * @param key
 * @param previousTable the previous table of the return table
 * @return pm_table* where key is in
 */
pm_table* PMLHash::searchPage(const uint64_t& key, pm_table** previousTable) {
  uint64_t idx = getRealHashIdx(key);

  pm_table* t = getNmTableFromIdx(idx);
  pm_table* pre = nullptr;
  while (1) {
    int pos = t->pos(key);
    if (pos >= 0) {
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
int PMLHash::insert(const uint64_t& key, const uint64_t& value) {
  if (checkDupKey(key)) {
    return -1;
  }

  uint64_t idx = getRealHashIdx(key);
  if (appendAutoOf(getNmTableFromIdx(idx), entry::makeEntry(key, value))) {
    split();
  }
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
int PMLHash::search(const uint64_t& key, uint64_t& value) {
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
int PMLHash::remove(const uint64_t& key) {
  // you should find its previous table
  pm_table* pre = nullptr;
  pm_table* t = searchPage(key, &pre);
  if (t == nullptr) {
    return -1;
  }
  for (int i = t->pos(key) + 1; i < t->fill_num; i++) {
    entry& kv = t->index(i);
    t->insert(kv.key, kv.value, i - 1);
  }
  if (--(t->fill_num) == 0 && pre != nullptr) {
    freeOverflowTable(t);
    pre->next_offset = NEXT_IS_NONE;
  }
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
int PMLHash::update(const uint64_t& key, const uint64_t& value) {
  entry* e = searchEntry(key);
  if (e == nullptr) {
    return -1;
  }
  e->value = value;
  return 0;
}

// -----------------------------------

int PMLHash::showPrivateData() {
  printf("-----\n");
  printf("PMLHASH Config:\n");
  printf("- start_addr:%p\n", start_addr);
  printf("- overflow_addr:%p\n", overflow_addr);
  printf("- meta_addr:%p\n", meta);
  printf("  - size:%zu level:%zu next:%zu overflow_num:%zu\n", meta->size,
         meta->level, meta->next, meta->overflow_num);
  printf("- table_addr:%p\n", table_arr);
  showKV("  - ");
  printf("- bitmap_addr:%p\n", bitmap);
  printf("-----\n");
}

int PMLHash::showKV(const char* prefix) {
  for (int i = 0; i < meta->size; i++) {
    pm_table* t = getNmTableFromIdx(i);
    printf("%sTable:%d ", prefix, i);
    while (1) {
      if (t->fill_num == 0) {
        printf("empty");
      }
      for (int j = 0; j < t->fill_num; j++) {
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
}

int PMLHash::showBitMap() {
  printf("%s\n", bitmap->to_string().c_str());
}
