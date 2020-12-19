#include "pmComponent.h"

void metadata::init() {
  bzero(this, sizeof(metadata));
  this->size = N_0;
}

void metadata::updateNextPtr() {
  if (++next >= N_LEVEL(level)) {
    level++;
    next = 0;
  }
}

int pm_table::init() {
  bzero(this, sizeof(pm_table));
  next_offset = NEXT_IS_NONE;
  return 0;
}

int pm_table::initArray(pm_table* start, int len) {
  for (int i = 0; i < len; i++) {
    (start++)->init();
  }
  return 0;
}

// return -1 if full, it won't go to the next_overflow_table
int pm_table::append(const uint64_t& key, const uint64_t& value) {
  if (fill_num == TABLE_SIZE) {
    return -1;
  }
  kv_arr[fill_num++] = entry::makeEntry(key, value);
  return 0;
}

// note: this will not increase fill_num, you have to set it yourself
int pm_table::insert(const uint64_t& key, const uint64_t& value, uint64_t pos) {
  if (pos >= TABLE_SIZE) {
    return -1;
  }
  kv_arr[pos] = entry::makeEntry(key, value);
  return 0;
}

// return -1 if not exists , never go to the overflow table
int pm_table::pos(const uint64_t& key) {
  for (size_t i = 0; i < fill_num; i++) {
    if (kv_arr[i].key == key) {
      return i;
    }
  }
  return -1;
}

entry* pm_table::index(int pos) {
  return &(kv_arr[pos]);
}

uint64_t pm_table::getValue(int pos) {
  return kv_arr[pos].value;
}

int pm_table::show() {
  printf("fill_num:%zu , next_offset:%zu :", fill_num, next_offset);
  for (auto& i : kv_arr) {
    printf("%zu,%zu| ", i.key, i.value);
  }
  printf("\n");
  return 0;
}
