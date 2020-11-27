#ifndef UTIL_H
#define UTIL_H
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include "pml_hash.h"

using namespace std;

// #define ASSERT_STEP 100 // we will use this if strict,but time-consuming
#define ASSERT_STEP 100000  // fast

#define INSERT 1
#define SEARCH 2
static map<std::string, uint64_t> TypeDict = {{"INSERT", INSERT}, {"READ", SEARCH}};

class Error : public exception {
 private:
  std::string msg;

 public:
  Error(std::string msg) : msg(msg) {}
  Error(const char* msg) : msg(msg) {}
  const char* what() const throw() { return msg.c_str(); };
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

// check and create file
bool chkAndCrtFile(const char* filePath);

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

int AssertTEST(PMLHash* f);
int BenchmarkYCSB(PMLHash* f, uint64_t n_thread = 1, string path_prefix = "");
int thread_routine(vector<std::pair<char, uint64_t>>& buf,
                   PMLHash* f,
                   uint64_t tid,
                   uint64_t n_thread);
int runYCSBBenchmark(const char* filePath, PMLHash* f);
int loadYCSBBenchmark(const char* filePath, PMLHash* f);
int runYCSBBenchmark(string filePath, PMLHash* f, uint64_t n_thread);

#endif