/**
 * * Created on 2020/12/19
 * * Editor: lwm
 * * this file will provide many functions for pmLinHash_test.cc
 */

#ifndef UTIL_H
#define UTIL_H
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include "../src/pmLinHash.h"
#include "../src/scheduler/scheduler.h"

using namespace std;

// #define ASSERT_STEP 100 // we will use this if strict,but time-consuming

#define INSERT 1
#define SEARCH 2
static map<std::string, uint64_t> TypeDict = {{"INSERT", INSERT},
                                              {"READ", SEARCH}};

// deprecated: use String deirectly
// class Error : public exception {
//  private:
//   std::string msg;

//  public:
//   Error(std::string msg) : msg(msg) {}
//   Error(const char* msg) : msg(msg) {}
//   const char* what() const throw() { return msg.c_str(); };
// };

#define raise(...)                                        \
  do {                                                    \
    printf(                                               \
        "%s[line %d] %s(): "                              \
        "%s"                                              \
        "\n",                                             \
        __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
    assert(0);                                            \
  } while (0);

int TestMultiThread(pmLinHash* f);
int TestBitMap(pmLinHash* f);
int TestHashRemove(pmLinHash* f);
int removeWithMsg(pmLinHash* f, uint64_t& key);
int TestHashUpdate(pmLinHash* f);
int updateWithMsg(pmLinHash* f, uint64_t& key, uint64_t& value);
int TestHashSearch(pmLinHash* f);
int searchWithMsg(pmLinHash* f, uint64_t& key, uint64_t& value);
int TestHashInsert(pmLinHash* f);
int insertWithMsg(pmLinHash* f, const uint64_t& key, const uint64_t& value);
pmLinHash* TestHashConstruct();

int AssertTEST(scheduler* sched);
int BenchmarkYCSBwithScheduler(scheduler* sched,
                               uint64_t n_thread = 1,
                               string path_prefix = "");
int worker(vector<std::pair<char, uint64_t>>& buf,
           scheduler* sched,
           uint64_t tid,
           uint64_t n_thread);
int loadYCSBBenchmark(const char* filePath, scheduler* sched);
int runYCSBBenchmark(string filePath, scheduler* sched, uint64_t n_thread = 1);

#endif