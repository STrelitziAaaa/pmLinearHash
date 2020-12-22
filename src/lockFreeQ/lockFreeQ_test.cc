#include "lockFreeQ.h"
#include <iostream>

constexpr int32_t n_thread = 10;
constexpr int32_t n_loop = 1000000;

int main() {
  syncQueue<uint64_t> q;
  uint64_t sum = 0;
  thread threads[10];
  for (int i = 0; i < n_thread; i++) {
    threads[i] = thread(
        [&q](int tid) {
          for (int i = 0; i < n_loop; i++) {
            // printf("thread:%d\n", tid);
            q << i;
          }
        },
        i);
  }
  for (int i = 0; i < n_thread * n_loop; i++) {
    uint64_t data;
    q >> data;
    sum += data;
  }
  for (int i = 0; i < n_thread; i++) {
    threads[i].join();
  }
  std::cout << sum << std::endl;
}