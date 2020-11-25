#ifndef UTIL_H
#define UTIL_H
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <string>
#include "pml_hash.h"

// check and create file
bool chkAndCrtFile(const char* filePath);

class Error {
 private:
  std::string msg;

 public:
  Error(std::string msg) : msg(msg) {}
  Error(const char* msg) : msg(msg) {}
  std::string string() { return msg; };
};

#define raise(...)                                        \
  do {                                                    \
    printf(                                               \
        "%s[line %d] %s(): "                              \
        "%s"                                              \
        "\n",                                             \
        __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
    assert(0);                                            \
  } while (0);

int runYCSBBenchmark(const char* filePath, PMLHash* f);
int loadYCSBBenchmark(const char* filePath, PMLHash* f);

int TestMultiThread(PMLHash* f);
int justInsertN(PMLHash* f, int n, uint64_t key);
int TestBitMap(PMLHash* f);
int TestHashRemove(PMLHash* f);
int removeWithMsg(PMLHash* f, uint64_t& key);
int TestHashUpdate(PMLHash* f);
int updateWithMsg(PMLHash* f, uint64_t& key, uint64_t& value);
int TestHashSearch(PMLHash* f);
int searchWithMsg(PMLHash* f, uint64_t& key, uint64_t& value);
int TestHashInsert(PMLHash* f);
int insertWithMsg(PMLHash* f, const uint64_t& key, const uint64_t& value);
PMLHash* TestHashConstruct();

#endif