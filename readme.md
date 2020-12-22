<img align="right" src="./static/db_logo.png" width=130px/>

# pmLinearHash
![](https://img.shields.io/badge/pmLinearHash-v0.1-519dd9.svg) ![](https://img.shields.io/badge/platform-linux-lightgray.svg) ![](https://img.shields.io/badge/c++-std=c++11-blue.svg) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)  
:rocket: This is an inplementation of [2020-Fall-DBMS-Project](https://github.com/ZhangJiaQiao/2020-Fall-DBMS-Project)  
> This branch is using scheduler

## TOC
<!-- TOC -->

- [pmLinearHash](#pmlinearhash)
  - [TOC](#toc)
  - [Scheduler](#scheduler)

<!-- /TOC -->
## Scheduler
- 通过scheduler将到来的请求串行处理,通过一个无锁队列实现请求的串化
  - 工作线程向队列发送数据,服务线程消费数据,根据操作类型执行对应的操作,并将结果返回给工作线程
- Example

  ```cpp
  int main() {
    pmLinHash f("./tmp");
    scheduler sched(&f, n_thread);
    thread server([&sched] { sched.RunAndServe(); });
    thread client1([&sched] { AssertTEST(&sched); });
    client1.join();

    BenchmarkYCSBwithScheduler(&sched, n_thread, "../");
    server.join();
  }
  ```
- Server端消费:
  - 通过TLS确定不同线程的error情况
  - 通过信号量sem_t同步
  ```cpp
  int RunAndServe() {
    printf("Server Run\n");
    while (1) {
      taskItem item;
      while (!chan.pop(item)) {
        if (!notDone.load()) {
          return 0;
        }
      };
      switch (item.op) {
        case OP_INIT:
          break;
        case OP_INSERT:
          f->insert(item.key, *(item.value));
          break;
        case OP_REMOVE:
          f->remove(item.key);
          break;
        case OP_SEARCH:
          f->search(item.key, item.value);
          break;
        case OP_UPDATE:
          f->update(item.key, *(item.value));
          break;
      }
    #ifdef PMDEBUG
      pmError_tls.println("db thread");
    #endif
      item.setError(pmError_tls);
      item.signal();
    }
  }
  ```
- Client端生产:
  - 将消息放入队列后,在该消息的sem_t上wait,等待服务端处理数据,服务端对该sem_t进行post,代表处理完成
  ```cpp
  int Do(TASK_OP op, const uint64_t& key, uint64_t* value = nullptr) {
    if (value == nullptr) {
      assert(op == OP_REMOVE);
    }

    taskItem item(op, key, value);
    defer guard([&item] { item.destroy(); });

    while (!chan.push(item)) {
    };
    item.wait();
  #ifdef PMDEBUG
    pmError_tls.println("user thread");
    item.error->println("user item");
  #endif
    pmError_tls.set(item.getError());

    return pmError_tls.hasError() ? -1 : 0;
  }
  ```
