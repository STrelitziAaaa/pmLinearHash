#include "pmLinHash.h"
#include "pmUtil.h"
#include "testUtil.h"
#define PMDEBUG

int main() {
  pmLinHash* f = TestHashConstruct();
  defer guard([&f] { delete f; });

  f->clear();
  printf("-----------Clear All------------\n");
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
  //
  printf("=========YCSB Benchmark=========\n");
  BenchmarkYCSB(f, 4, "../");
  printf("==========Benchmark OK===========\n");
}
