/**
 * * Created on 2020/12/19
 * * Editor: lwm
 * * this file is to define some subclass using in the pmLinHash,like
 * metadata,pm_table,entry,bitmap
 */

#ifndef __PMCOMPONENT_H__
#define __PMCOMPONENT_H__

#include <math.h>
#include <memory.h>
#include <bitset>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include "pmError.h"
#include "pmGlobal.h"

using namespace std;

typedef struct metadata {
  size_t size;            // the size of whole hash table array
  size_t level;           // level of hash
  uint64_t next;          // the index of the next split hash table
  uint64_t overflow_num;  // amount of overflow hash tables

 public:
  void init();
  void updateNextPtr();
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
  int init();
  static int initArray(pm_table* start, int len);
  int append(const uint64_t& key, const uint64_t& value);
  int insert(const uint64_t& key, const uint64_t& value, uint64_t pos);
  int pos(const uint64_t& key);
  entry* index(int pos);
  uint64_t getValue(int pos);
  int show();
} pm_table;

typedef struct bitmap_st {
  std::bitset<BITMAP_SIZE> bitmap;  // 1: available 0: occupied

 public:
  void init() { bitmap.set(); }
  void set(int pos) { bitmap.set(pos, 0); }
  string to_string() { return bitmap.to_string(); }
  void reset(int pos) { bitmap.set(pos, 1); }
  size_t findFirstAvailable() { return bitmap._Find_first(); }
} bitmap_st;

#endif  // __PMCOMPONENT_H__