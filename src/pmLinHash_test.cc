#include "../src/pmLinHash.h"
#include "../src/scheduler/scheduler.h"
#include "testUtil.h"

// g++ benchmark.cc util.cc ../src/pmLinHash.cc ../src/pmComponent.cc
// ../src/pmError.cc  -o benchmark -lpthread -std=c++17 -lpmem

constexpr int n_thread = 4;
#define PMDEBUG
int main() {
  pmLinHash f("./tmp");
  scheduler sched(&f, n_thread);
  thread server([&sched] { sched.RunAndServe(); });

  thread client1([&sched] { AssertTEST(&sched); });
  client1.join();

  BenchmarkYCSBwithScheduler(&sched, n_thread, "../");
  
  sched.AllDone();
  server.join();
}
