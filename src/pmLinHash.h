#ifndef PML_HASH_H
#define PML_HASH_H
#include <libpmem.h>
#include <bitset>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include "pmComponent.h"
#include "pmGlobal.h"
#include "pmUtil.h"

using namespace std;

// persistent memory linear hash
class pmLinHash {
 private:
  void* start_addr;     // the start address of mapped file
  void* overflow_addr;  // the start address of overflow tableArray
  metadata* meta;       // virtual address of metadata
  pm_table* table_arr;  // virtual address of hash table array
  bitmap_st* bitmap;    // for gc

  void split();
  uint64_t hashFunc(const uint64_t& key, const size_t& hash_size);
  // pm_table* newOverflowTable();
  pm_table* newOverflowTable(pm_table* pre = nullptr);
  int freeOverflowTable(uint64_t idx);
  int freeOverflowTable(pm_table* t);
  pm_table* newNormalTable();
  pm_table* getNmTableFromIdx(uint64_t idx);
  pm_table* getOfTableFromIdx(uint64_t idx);
  uint64_t getIdxFromTable(pm_table* start, pm_table* t);
  uint64_t getIdxFromOfTable(pm_table* t);
  uint64_t getIdxFromNmTable(pm_table* t);

  bool checkDupKey(const uint64_t& key);

  uint64_t splitOldIdx();
  uint64_t splitNewIdx();
  uint64_t getRealHashIdx(const uint64_t& key);
  int appendAutoOf(pm_table* startTable, const entry& kv);
  int insertAutoOf(pm_table* startTable, const entry& kv, uint64_t pos);
  int updateAfterSplit(pm_table* startTable, uint64_t fill_num);
  int cntTablesSince(pm_table* startTable);

  entry* searchEntry(const uint64_t& key);
  pm_table* searchEnteyWithPage(const uint64_t& key,
                                uint64_t* pos = nullptr,
                                pm_table** previousTable = nullptr);

 public:
  pmLinHash() = delete;
  pmLinHash(const char* file_path);
  ~pmLinHash();

  int insert(const uint64_t& key, const uint64_t& value);
  int search(const uint64_t& key, uint64_t& value);
  int search(const uint64_t& key, uint64_t* value);
  int remove(const uint64_t& key);
  int update(const uint64_t& key, const uint64_t& value);
  int clear();

  int initMappedMem();
  int recoverMappedMen();

  int showConfig();
  int showKV(const char* prefix = "");
  int showBitMap();
};

#endif