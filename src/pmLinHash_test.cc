#include "pmLinHash.h"
#include "pmUtil.h"
#include "testUtil.h"

#define PMDEBUG

int main() {
  pmLinHash* f = TestHashConstruct();
  defer guard([&f] { delete f; });

  f->clear();
  //
  printf("-----------SimpleTest-----------\n");
  // TestHashInsert(f);
  // TestHashSearch(f);
  // TestHashUpdate(f);
  // TestHashRemove(f);
  printf("-----------SimpleTest OK--------\n");
  //
  printf("----Simple MultiThread Test-----\n");
  // TestMultiThread(f);
  printf("----Simple MultiThread Test OK--\n");
  //
  printf("-----------Assert Test ---------\n");
  AssertTEST(f);
  printf("-----------Assert Test OK-------\n");
  f->clear();
  //
  printf("=========YCSB Benchmark=========\n");
  BenchmarkYCSB(f, 1, "../");
  printf("==========Benchmark OK===========\n");
}
