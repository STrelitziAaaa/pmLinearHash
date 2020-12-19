#include "pmComponent.h"

/**
 * @brief init metadata,just memset to zero, and size to N_0
 * @return (void)
 */
void metadata::init() {
  bzero(this, sizeof(metadata));
  this->size = N_0;
}

/**
 * @brief it will increase level, only call it after split()
 * @return (void)
 */
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

/**
 * @brief static function used to init an array of table
 * @param start start addr of the first table
 * @param len number of tables
 * @return  nothing
 */
int pm_table::initArray(pm_table* start, int len) {
  for (int i = 0; i < len; i++) {
    (start++)->init();
  }
  return 0;
}

/**
 * @brief append to the end of the table, it never goes the next page
 * @param key
 * @param value
 * @return return -1 if full
 */
int pm_table::append(const uint64_t& key, const uint64_t& value) {
  if (fill_num == TABLE_SIZE) {
    return -1;
  }
  kv_arr[fill_num++] = entry::makeEntry(key, value);
  return 0;
}

/**
 * @brief insert to the exact postion, it never changes fill_num
 * @param key
 * @param value
 * @param pos
 * @return return -1 if pos >= TABLE_SIZE
 */
int pm_table::insert(const uint64_t& key, const uint64_t& value, uint64_t pos) {
  if (pos >= TABLE_SIZE) {
    return -1;
  }
  kv_arr[pos] = entry::makeEntry(key, value);
  return 0;
}

/**
 * @brief get the position of key in a table, it never goes to the next page
 * @param key
 * @return return -1 if not exists
 */
int pm_table::pos(const uint64_t& key) {
  for (size_t i = 0; i < fill_num; i++) {
    if (kv_arr[i].key == key) {
      return i;
    }
  }
  return -1;
}

/**
 * @brief get the entry addr at the position
 * @param pos
 * @return return nullptr if pos >= TABLE_SIZE
 */
entry* pm_table::index(int pos) {
  if (pos >= TABLE_SIZE) {
    return nullptr;
  }
  return &(kv_arr[pos]);
}

/**
 * @brief get the value of the entry at the position
 * @param pos
 * @return return VALUE_NOT_FOUND if pos >= TABLE_SIZE, so it's really foolish
 * to use uint64_t
 */
uint64_t pm_table::getValue(int pos) {
  if (pos >= TABLE_SIZE) {
    return VALUE_NOT_FOUND;
  }
  return kv_arr[pos].value;
}

/**
 * @brief print the table, used in test
 * @return nothing
 */
int pm_table::show() {
  printf("fill_num:%zu , next_offset:%zu :", fill_num, next_offset);
  for (auto& i : kv_arr) {
    printf("%zu,%zu| ", i.key, i.value);
  }
  printf("\n");
  return 0;
}
