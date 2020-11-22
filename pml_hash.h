#ifndef PML_HASH_H
#define PML_HASH_H

#include <libpmem.h>
#include <math.h>
#include <memory.h>
#include <stdint.h>
#include <unistd.h>
#include <iostream>
#include <vector>

#define TABLE_SIZE 4                                // adjustable
#define HASH_SIZE 16                                // adjustable
#define FILE_SIZE 1024 * 1024 * 16                  // 16 MB adjustable
#define BITMAP_SIZE 1024 * 1024 / sizeof(pm_table)  // for gc of overflow table
#define N_0 4
#define N_LEVEL(level) (pow(2, level) * N_0)

// typically we will use -1,but unfortunately the dataType is uint64_t
#define NEXT_IS_NONE 999999999
#define VALUE_NOT_FOUND NEXT_IS_NONE
using namespace std;

typedef struct metadata {
  size_t size;            // the size of whole hash table array
  size_t level;           // level of hash
  uint64_t next;          // the index of the next split hash table
  uint64_t overflow_num;  // amount of overflow hash tables

 public:
  int init() {
    bzero(this, sizeof(metadata));
    this->size = N_0;
  }
  int updateNextPtr() {
    if (++next >= N_LEVEL(level)) {
      level++;
      next = 0;
    }
  }
} metadata;

// data entry of hash table
typedef struct entry {
  uint64_t key;
  uint64_t value;

 public:
  static entry makeEntry(const uint64_t& key, const uint64_t& value) {
    return entry{key, value};
  }
} entry;

// hash table
typedef struct pm_table {
  entry kv_arr[TABLE_SIZE];  // data entry array of hash table
  uint64_t fill_num;         // amount of occupied slots in kv_arr
  uint64_t
      next_offset;  // the offset of overflow hash table regarding overflow_addr
 public:
  int init() {
    bzero(this, sizeof(pm_table));
    next_offset = NEXT_IS_NONE;
  }
  static int initArray(pm_table* start, int len) {
    for (int i = 0; i < len; i++) {
      (start++)->init();
    }
  }
  // return -1 if full, it won't go to the next_overflow_table
  int append(const uint64_t& key, const uint64_t& value) {
    if (fill_num == TABLE_SIZE) {
      return -1;
    }
    kv_arr[fill_num++] = entry::makeEntry(key, value);
    return 0;
  }
  // note: this will not increase fill_num, you have to set it yourself
  int insert(const uint64_t& key, const uint64_t& value, uint64_t pos) {
    if (pos >= TABLE_SIZE) {
      return -1;
    }
    kv_arr[pos] = entry::makeEntry(key, value);
    return 0;
  }
  // return -1 if not exists , never go to the overflow table
  int pos(const uint64_t& key) {
    for (int i = 0; i < fill_num; i++) {
      if (kv_arr[i].key == key) {
        return i;
      }
    }
    return -1;
  }
  entry& index(int pos) { return kv_arr[pos]; }
  uint64_t getValue(int pos) { return kv_arr[pos].value; }
  // entry* getEntryPtr(int idx) { return &kv_arr[idx]; }
} pm_table;

// persistent memory linear hash
class PMLHash {
 private:
  void* start_addr;     // the start address of mapped file
  void* overflow_addr;  // the start address of overflow table
                        // array
  metadata* meta;       // virtual address of metadata
  pm_table* table_arr;  // virtual address of hash table array

  uint8_t* bitmap;

  void split();
  uint64_t hashFunc(const uint64_t& key, const size_t& hash_size);
  pm_table* newOverflowTable();
  pm_table* newOverflowTable(pm_table* pre);
  pm_table* newNormalTable();
  pm_table* getNmTableFromIdx(uint64_t idx);
  pm_table* getOfTableFromIdx(uint64_t idx);

  uint64_t splitOldIdx();
  uint64_t splitNewIdx();
  uint64_t getRealHashIdx(const uint64_t& key);
  int appendAutoOf(pm_table* startTable, const entry& kv);
  int insertAutoOf(pm_table* startTable, const entry& kv, uint64_t pos);
  int updateAfterSplit(pm_table* startTable, uint64_t fill_num);
  int cntTablesSince(pm_table* startTable);

  entry* searchEntry(const uint64_t& key);
  pm_table* searchPage(const uint64_t& key, pm_table** previousTable);

 public:
  PMLHash() = delete;
  PMLHash(const char* file_path);
  ~PMLHash();

  int insert(const uint64_t& key, const uint64_t& value);
  int search(const uint64_t& key, uint64_t& value);
  int remove(const uint64_t& key);
  int update(const uint64_t& key, const uint64_t& value);

  int showPrivateData();
  int showKV(const char* prefix = "");
};

#endif