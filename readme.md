<img align="right" src="./static/db_logo.png" width=130px/>

# pmLinearHash
![](https://img.shields.io/badge/pmLinearHash-v0.1-519dd9.svg) ![](https://img.shields.io/badge/platform-linux-lightgray.svg) ![](https://img.shields.io/badge/c++-std=c++11-blue.svg) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)  
:rocket: This is an inplementation of [2020-Fall-DBMS-Project](https://github.com/ZhangJiaQiao/2020-Fall-DBMS-Project)  
> This branch is using scheduler

## TOC
<!-- TOC -->

- [pmLinearHash](#pmlinearhash)
  - [TOC](#toc)
  - [Performance](#performance)
  - [Scheduler](#scheduler)

<!-- /TOC -->
## Performance
__性能孱弱__
```yaml
=========YCSB Benchmark=======
注:时间基本为纯Insert/Search时间,不包括读文件   n_thread=4
-----------Clear All------------
load .././benchmark/10w-rw-0-100-load.txt       WTime: 1084.831900ms    OPS: 0.092181M  Delay: 10.848211ns
run .././benchmark/10w-rw-0-100-load.txt        WTime: 1636.409200ms    OPS: 0.061110M  Delay: 16.363928ns
-----------Clear All------------
load .././benchmark/10w-rw-25-75-load.txt       WTime: 1225.126600ms    OPS: 0.081625M  Delay: 12.251143ns
run .././benchmark/10w-rw-25-75-load.txt        WTime: 1647.453300ms    OPS: 0.060700M  Delay: 16.474368ns
-----------Clear All------------
load .././benchmark/10w-rw-50-50-load.txt       WTime: 1130.629400ms    OPS: 0.088447M  Delay: 11.306181ns
run .././benchmark/10w-rw-50-50-load.txt        WTime: 1640.705200ms    OPS: 0.060950M  Delay: 16.406888ns
-----------Clear All------------
load .././benchmark/10w-rw-75-25-load.txt       WTime: 1127.817400ms    OPS: 0.088668M  Delay: 11.278061ns
run .././benchmark/10w-rw-75-25-load.txt        WTime: 1637.446900ms    OPS: 0.061071M  Delay: 16.374305ns
-----------Clear All------------
load .././benchmark/10w-rw-100-0-load.txt       WTime: 1201.648800ms    OPS: 0.083220M  Delay: 12.016368ns
run .././benchmark/10w-rw-100-0-load.txt        WTime: 1634.943600ms    OPS: 0.061165M  Delay: 16.349273ns
-----------Clear All------------
==========Benchmark OK=========
```
## Scheduler
- 通过scheduler将到来的请求串行处理,通过一个无锁队列实现请求的串化
  - 工作线程向队列发送数据,服务线程消费数据,根据操作类型执行对应的操作,并将结果返回给工作线程
- Example

  ```cpp
  int main() {
    pmLinHash f("./tmp");
    scheduler sched(&f, n_thread);
    // run server
    thread server([&sched] { sched.RunAndServe(); });
    // run client1: 进行程序正确性检验
    thread client1([&sched] { AssertTEST(&sched); });
    client1.join();
    // run benchmark: 性能测试
    BenchmarkYCSBwithScheduler(&sched, n_thread, "../");
    // 告知服务端退出
    server.AllDone()
    // 等待服务端退出
    server.join();
  }
  ```
- Server端消费:
  - 通过TLS确定不同线程的error情况
  - 通过信号量sem_t同步
  - 简易逻辑
    ```cpp
    int RunAndServe() {
    while (1) {
      taskItem item;
      while (!chan.pop(item)) {
        if (!notDone.load()) {
          break;
        }
        switch(item.op){
          //do insert/search/update/remove
        }
        // 告知客户端操作完成
        item.signal();
      };
    ```

  <details>
  <summary>源码</summary>

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
  </details>
- Client端生产:
  - 将消息放入队列后,在该消息的sem_t上wait,等待服务端处理数据,服务端对该sem_t进行post,代表处理完成
  - 简易逻辑
    ```cpp
    int Do(TASK_OP op, const uint64_t& key, uint64_t* value = nullptr) {
      taskItem item(op, key, value);
      while (!chan.push(item)) {
      };
      // 等待服务端执行操作
      item.wait();
    }
    ```
  <details>
  <summary>源码</summary>

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
  </details>