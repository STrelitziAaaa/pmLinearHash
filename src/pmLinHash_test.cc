#include "pmLinHash.h"
#include "util.h"

// g++ pml_hash_testing.cc pml_hash.cc util.cc -o pml_hash_testing -lpmem
// -lpthread

int main() {
  // you have to return the original object,instead of use `=` in
  // TestHashConstruct(&f) because it will call `unmap` if it die;
  pmLinHash* f = TestHashConstruct();
  // AssertTEST(f);
  f->clear();
  printf("-----------Clear All----------\n");
  TestHashInsert(f);
  TestHashSearch(f);
  TestHashUpdate(f);
  TestHashRemove(f);
  // f->showBitMap();
  // f->showKV();
  // f->showBitMap();
  // justInsertN(f, 10, 5);

  // TestMultiThread(f);

  // BenchmarkYCSB(f, 4, "../");
  delete f;
}
