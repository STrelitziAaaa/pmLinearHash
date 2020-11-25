#include <sstream>
#include <thread>
#include "pml_hash.h"
#include "util.h"

int main() {
  // you have to return the original object,instead of use `=` in
  // TestHashConstruct(&f) because it will call `unmap` if it die;

  PMLHash* f = TestHashConstruct();
  // f->showBitMap();
  // TestHashInsert(f);
  // TestHashSearch(f);
  // TestHashUpdate(f);
  // TestHashRemove(f);
  // f->showBitMap();
  // f->remove(15);
  // f->showKV();
  // f->showBitMap();
  // justInsertN(f, 10, 5);
  // TestMultiThread(f);
  loadYCSBBenchmark("./benchmark/10w-rw-0-100-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-0-100-load.txt", f);
  f->clear();
  loadYCSBBenchmark("./benchmark/10w-rw-25-75-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-25-75-load.txt", f);
  f->clear();
  loadYCSBBenchmark("./benchmark/10w-rw-50-50-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-50-50-load.txt", f);
  f->clear();
  loadYCSBBenchmark("./benchmark/10w-rw-75-25-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-75-25-load.txt", f);
  f->clear();
  loadYCSBBenchmark("./benchmark/10w-rw-100-0-load.txt", f);
  runYCSBBenchmark("./benchmark/10w-rw-100-0-load.txt", f);
  f->clear();
}
