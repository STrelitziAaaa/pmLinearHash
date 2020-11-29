
# pmLinearHash

[TOC]

## Intro
To inplement index in a DBMS, we have two main methods:
  - b/b+ tree
  - hash

To implement hash alg., we have different kinds of schemes. depending on whether the number of buckets is fixed at runtime
  - Static Hashing Schemes
    - Linear Probe Hashing
    - Robin Hood Hashing
    - Cuckoo Hashing
  - Dynamic Hashing Schemes
    - Chained Hashing
    - Extensible Hashing
    - Linear Hashing

Here we will implement `Linear Hashing`

> note: The code in the project is mixed with C and C++
## Build&Run
```bash
cd build
make
./pmlhash
```
## PMDK Env
### Search
  - `apt search libpmem`
  - 可以看到很多相关的包,按官网选择`-dev`用于开发即可
### Install
  - `sudo apt install libpmem-dev`
### Import
  - `#include <libpmem.h>`

## Function
- 增删查改
  - 测试代码见 [AssertTEST()](./util.cc#L264) ,下为简单示例
  ```c
  PMLHash* f = new PMLHash("./pmemFile");
  uint64_t key = 1, value_in = 1;
  // insert
  assert(f->insert(key, value_in) != -1);
  assert(f->insert(key, value_in) == -1);
  // search
  uint64_t value_out = 0;
  assert(f->search(key, value_out) != -1);
  assert(value_out == value_in);
  // update
  assert(f->update(key, value_new) == -1);
  assert(f->search(key, value_out) != -1);
  assert(value_out == value_new);
  // remove
  assert(f->remove(key) == -1);
  assert(f->search(key, value_out) == -1);
  // clear
  assert(f->clear() != -1);
  assert(f->search(key,value_out) == -1);
  ```

- GC
  - 溢出表根据 `bitmap` 位示图进行分配回收
  - `std::bitset<BITMAP_SIZE> bitmap` 位于pm中,以保证持久性
- MultiThread
  - 简单使用粗粒度的锁(每个基础操作[insert/search/update/remove]加锁)
  - `std::recursive_mutex mutx` 位于普通内存(stack or heap)中,属于运行时才用到
  - 待完善多线程YCSB测试后,准备优化一下,给哈希桶以锁(更极端的,给每个哈希页以锁),这样能一定程度增加并行度

## 单线程YCSB性能测试
- 起初采用了一边读一边insert/search的方法,然后对getline的循环测流逝时间
- 后来考虑到多线程性能测试的需要,因为多线程不可能让每个线程去并行读硬盘(总线总是串行的),所以改用了先把数据读进内存缓冲区,再insert/search
- 时间基本为纯的insert/search时间
  ```c
  auto t_start = std::chrono::high_resolution_clock::now();
  for (auto& key : keyBuf) {
    if (key.first == INSERT) {
      f->insert(key.second, key.second);
    } else {
      uint64_t value;
      assert(f->search(key.second, value) != -1);
      assert(value == key.second);
    }
  }
  auto t_end = std::chrono::high_resolution_clock::now();
  ```
```yaml
=========YCSB Benchmark=======
注:时间基本为纯Insert/Search时间,不包括读文件 n_thread=1
load .././benchmark/10w-rw-0-100-load.txt       WTime: 107.249 ms       OPS: 0.932422M  Delay: 1.07248 ns
run .././benchmark/10w-rw-0-100-load.txt        WTime: 26.7503 ms       OPS: 3.73831M   Delay: 0.2675 ns
load .././benchmark/10w-rw-25-75-load.txt       WTime: 105.719 ms       OPS: 0.945909M  Delay: 1.05718 ns
run .././benchmark/10w-rw-25-75-load.txt        WTime: 26.7517 ms       OPS: 3.73812M   Delay: 0.267514 ns
load .././benchmark/10w-rw-50-50-load.txt       WTime: 108.684 ms       OPS: 0.92011M   Delay: 1.08683 ns
run .././benchmark/10w-rw-50-50-load.txt        WTime: 26.8354 ms       OPS: 3.72646M   Delay: 0.268351 ns
load .././benchmark/10w-rw-75-25-load.txt       WTime: 104.998 ms       OPS: 0.95241M   Delay: 1.04997 ns
run .././benchmark/10w-rw-75-25-load.txt        WTime: 26.9531 ms       OPS: 3.71019M   Delay: 0.269528 ns
load .././benchmark/10w-rw-100-0-load.txt       WTime: 107.042 ms       OPS: 0.93422M   Delay: 1.07041 ns
run .././benchmark/10w-rw-100-0-load.txt        WTime: 27.2577 ms       OPS: 3.66872M   Delay: 0.272574 ns
==========Benchmark OK=========
```
## 多线程YCSB性能测试
```yaml
=========YCSB Benchmark=======
注:时间基本为纯Insert/Search时间,不包括读文件 n_thread=10
load .././benchmark/10w-rw-0-100-load.txt       WTime: 105.956 ms       OPS: 0.943799M  Delay: 1.05955 ns
run .././benchmark/10w-rw-0-100-load.txt        WTime: 111.341 ms       OPS: 0.898153M  Delay: 1.1134 ns
load .././benchmark/10w-rw-25-75-load.txt       WTime: 105.453 ms       OPS: 0.948298M  Delay: 1.05452 ns
run .././benchmark/10w-rw-25-75-load.txt        WTime: 120.667 ms       OPS: 0.828738M  Delay: 1.20665 ns
load .././benchmark/10w-rw-50-50-load.txt       WTime: 109.775 ms       OPS: 0.910967M  Delay: 1.09773 ns
run .././benchmark/10w-rw-50-50-load.txt        WTime: 111.913 ms       OPS: 0.893561M  Delay: 1.11912 ns
load .././benchmark/10w-rw-75-25-load.txt       WTime: 106.694 ms       OPS: 0.937266M  Delay: 1.06693 ns
run .././benchmark/10w-rw-75-25-load.txt        WTime: 116.1 ms         OPS: 0.861331M  Delay: 1.16099 ns
load .././benchmark/10w-rw-100-0-load.txt       WTime: 107.989 ms       OPS: 0.926033M  Delay: 1.07988 ns
run .././benchmark/10w-rw-100-0-load.txt        WTime: 111.037 ms       OPS: 0.900613M  Delay: 1.11035 ns
==========Benchmark OK=========
```
### Comment
多线程由于有锁的竞态,导致其实效率很低下,这说明我的设计不太好. 另一方面,这个性能测试的结果随时间差异很大,这可能与cpu的状态有关  
但无论如何,测试发现,单线程都要优于简单粗粒度加锁的多线程,这可能是因为数据集过少,导致的write操作过快的原因;  
可以准备将锁换成读写锁,这样可以在100%读的测试中看出多线程的优势!


### 多线程读写锁
很奇怪的,理论上,加入读写锁,应该减少`run WTime`才对,但测试中却增加了;
```yaml
=========YCSB Benchmark=======
注:时间基本为纯Insert/Search时间,不包括读文件   n_thread=10
load .././benchmark/10w-rw-0-100-load.txt       WTime: 105.390400ms     OPS: 0.948863M  Delay: 1.053893ns
run .././benchmark/10w-rw-0-100-load.txt        WTime: 156.114400ms     OPS: 0.640562M  Delay: 1.561128ns
load .././benchmark/10w-rw-25-75-load.txt       WTime: 103.427700ms     OPS: 0.966869M  Delay: 1.034267ns
run .././benchmark/10w-rw-25-75-load.txt        WTime: 151.276900ms     OPS: 0.661046M  Delay: 1.512754ns
load .././benchmark/10w-rw-50-50-load.txt       WTime: 103.090000ms     OPS: 0.970036M  Delay: 1.030890ns
run .././benchmark/10w-rw-50-50-load.txt        WTime: 152.888800ms     OPS: 0.654077M  Delay: 1.528873ns
load .././benchmark/10w-rw-75-25-load.txt       WTime: 104.230100ms     OPS: 0.959425M  Delay: 1.042291ns
run .././benchmark/10w-rw-75-25-load.txt        WTime: 148.639800ms     OPS: 0.672774M  Delay: 1.486383ns
load .././benchmark/10w-rw-100-0-load.txt       WTime: 103.514100ms     OPS: 0.966062M  Delay: 1.035131ns
run .././benchmark/10w-rw-100-0-load.txt        WTime: 147.955100ms     OPS: 0.675887M  Delay: 1.479536ns
```

### Fine-grained mutex
所谓细粒度的锁,可以认为就是同一时间会有多个线程在整个索引数据结构中
- 事实上,以桶(含初始页和溢出页)为单位的锁是难以实现的,因为要上锁,就要获取key对应的桶的序号,但很有可能这个序号是错误的!,即马上其他线程的插入导致meta->level++了,那么,我们就要保证我们获取桶序号到插入元素之间是被锁保护的,但是又必须获取到桶序号才能为那个桶上锁,所以矛盾出现了! 
- 细粒度的锁面临的问题
  - 其他线程插入导致的level++,本线程插入了旧的桶,导致后续的search找不到元素
  - 加锁的时刻是访问第一个normal_table时加锁,而不是insert/search这些操作,因为insert会触发split(),从而访问其他的桶