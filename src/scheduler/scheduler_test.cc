
#include "scheduler.h"
#include <iostream>
#include <thread>

using namespace std;

constexpr int n_thread = 4;
constexpr int n_loop = 4;

void searchWithMsg(scheduler* sched, int tid) {
  for (size_t i = 0; i < n_loop; i++) {
    uint64_t v;
    if (sched->Do(OP_SEARCH, i, &v) < 0) {
      printf("[tid:%d] search:%zu ,value:%zu Err:%s\n", tid, i, v,
             pmError_tls.c_str());
    } else {
      printf("[tid:%d] search:%zu ,value:%zu\n", tid, i, v);
    }
  }
}

void updateWithMsg(scheduler* sched, int tid) {
  for (size_t i = 0; i < n_loop; i++) {
    uint64_t v = i + 1;
    if (sched->Do(OP_UPDATE, i, &v) < 0) {
      printf("[tid:%d] update:%zu ,value updated:%zu Err:%s\n", tid, i, v,
             pmError_tls.c_str());
    } else {
      printf("[tid:%d] update:%zu ,value updated:%zu \n", tid, i, v);
    }
  }
}

void insertWithMsg(scheduler* sched, int tid) {
  for (size_t i = 0; i < n_loop; i++) {
    if (sched->Do(OP_INSERT, i, &i) < 0) {
      printf("[tid:%d] insert:%zu Err:%s\n", tid, i, pmError_tls.c_str());
    } else {
      printf("[tid:%d] insert:%zu\n", tid, i);
    }
  }
}

int main() {
  pmLinHash f("./tmp");
  scheduler sched(&f, n_thread);
  thread threads[n_thread];

  for (int i = 0; i < n_thread; i++) {
    threads[i] = thread(
        [&sched](int tid) {
          insertWithMsg(&sched, tid);
          searchWithMsg(&sched, tid);
          updateWithMsg(&sched, tid);
          searchWithMsg(&sched, tid);
          sched.Done();
        },
        i);
  }
  sched.RunAndServe();
  printf("Serve OK: Wait Child\n");
  for (auto& t : threads) {
    t.join();
  }
}