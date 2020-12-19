<img align="right" src="./static/db_logo.png" width=110px/>

# pmLinearHash
![](https://img.shields.io/badge/pmLinearHash-v0.1-519dd9.svg) ![](https://img.shields.io/badge/platform-linux-lightgray.svg) ![](https://img.shields.io/badge/c++-std=c++17-blue.svg) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)  
:rocket: This is an inplementation of [2020-Fall-DBMS-Project](https://github.com/ZhangJiaQiao/2020-Fall-DBMS-Project)

## TOC
<!-- TOC -->

- [pmLinearHash](#pmlinearhash)
  - [TOC](#toc)
  - [Todo](#todo)
  - [Intro](#intro)
  - [Env Config](#env-config)
    - [NVM](#nvm)
    - [PMDK](#pmdk)
  - [Build&Run](#buildrun)
  - [Feature](#feature)
    - [CRUD](#crud)
    - [GC](#gc)
    - [MultiThread](#multithread)
    - [Doc](#doc)
  - [单线程YCSB性能测试](#单线程ycsb性能测试)
  - [多线程YCSB性能测试](#多线程ycsb性能测试)
  - [Comment](#comment)
    - [About CRUD](#about-crud)
    - [About MultiThread](#about-multithread)
  - [Concurrency safe](#concurrency-safe)
    - [Fine-grained mutex](#fine-grained-mutex)
  - [Unsolved questions](#unsolved-questions)

<!-- /TOC -->
## Todo
- [ ] NVM环境配置
- [ ] 进入/build编译,再进入/bin运行,过于繁琐
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



## Env Config
### NVM
- todo
### PMDK
- Install
  - `sudo apt install libpmem-dev`

- Import
  - `#include <libpmem.h>`

## Build&Run
The Repo was built&run&tested under the following platform
- Linux 4.19.104-microsoft-standard x86_64 GNU/Linux
```bash
cd build
cmake ..
make
cd ../bin
./pmlhash
```

## Feature
### CRUD
- Insert
  - 直接在桶上 `append` ,根据返回值判断是否需要 `split` ,如果 `split` ,则通过遍历原桶,向原桶和新桶分发(`insert`)元素,通过维护计数器来确定元素插入位置,整个过程是 `inplace` 的
- Remove
  - 通过 `searchWithPage` 函数,返回被查元素`所在页`和`前一页`,删除时,直接将所在页最后一个元素与被删元素 `swap` ,并递减 `fillnum` 即可
- Search
  - 直接返回被查元素的引用(指针)
- Update
  - 直接返回被查元素的引用(指针),然后修改
  <details>
  <summary>Insert</summary>

  ```c
  int PMLHash::insert(const uint64_t& key, const uint64_t& value) {
    std::unique_lock wr_lock(rwMutx);

    if (searchEntry(key) != nullptr) {
      pmError.set(DupKeyErr);
      return -1;
    }

    try {
      if (append Of(getNmTableFromIdx(getRealHashIdx(key)),
                      entry::makeEntry(key, value))) {
        split();
      }
    } catch (Error& e) {
      pmError.perror("insert");
      return -1;
    }
    pmem_persist(start_addr, FILE_SIZE);
    return 0;
  }
  ```
  </details>

  <details>
  <summary>Remove</summary>

  ```c
  int PMLHash::remove(const uint64_t& key) {
    std::unique_lock wr_lock(rwMutx);
    // you should find its previous table
    pm_table* pre = nullptr;
    pm_table* t = searchPage(key, &pre);
    if (t == nullptr) {
      pmError.set(NotFoundErr);
      return -1;
    }

    entry& lastKV = t->index(t->fill_num - 1);
    t->insert(lastKV.key, lastKV.value, t->pos(key));

    while (t->next_offset != NEXT_IS_NONE) {
      pre = t;
      t = getOfTableFromIdx(t->next_offset);
      entry& lastKV = t->index(t->fill_num - 1);
      assert(pre->fill_num == TABLE_SIZE);
      pre->insert(lastKV.key, lastKV.value, TABLE_SIZE - 1);
    }

    if (--(t->fill_num) == 0 && pre != nullptr) {
      freeOverflowTable(t);
      pre->next_offset = NEXT_IS_NONE;
    }
    pmem_persist(start_addr, FILE_SIZE);

    return 0;
  }
  ```
  </details>

  <details>
  <summary>Search</summary>

  ```c
  int PMLHash::search(const uint64_t& key, uint64_t& value) {
    std::shared_lock rd_lock(rwMutx);

    entry* e = searchEntry(key);
    if (e == nullptr) {
      return -1;
    }
    value = e->value;
    return 0;
  }
  ```
  </details>

  <details>
  <summary>Update</summary>

  ```c
  int PMLHash::update(const uint64_t& key, const uint64_t& value) {
    std::unique_lock wr_lock(rwMutx);

    entry* e = searchEntry(key);
    if (e == nullptr) {
      return -1;
    }

    e->value = value;
    pmem_persist(e, sizeof(entry));
    return 0;
  }
  ```
  </details>

  <details>
  <summary>Use example</summary>

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

  </details>
  <details>
  <summary>AssertTest Result</summary>
  <a href="./util.cc#L262">src code</a>
  
  ```yaml
  =========Assert Test==========
  TEST 300000 KVs
  Check Empty Env...
  Check Insert&Search...
  Check Update&Search...
  Check Remove&Search...
  Check Clear&Search...
  =========Assert OK============
  ```
  </details>
  <details> 
  <summary>Simple mock with 16kv and table_size=1</summary>

  ```c
  pmFile Exists:Recover
  mapped_len:16777216 , 16.000000 mb, ptr:0x7ff94e000000
  =========PMLHash Config=======
  - start_addr:0x7ff94e000000
  - overflow_addr:0x7ff94e800000
  - meta_addr:0x7ff94e000000
    - size:4 level:0 next:0 overflow_num:0
  - table_addr:0x7ff94e001020
  ===========Table View=========
    - Table:0 empty
    - Table:1 empty
    - Table:2 empty
    - Table:3 empty
  ===========Table OK===========
  - bitmap_addr:0x7ff94e000020
  =========Init Config=======
  TABLE_SIZE:1
  HASH_SIZE:16
  FILE_SIZE:16777216
  BITMAP_SIZE:32768
  OVERFLOW_TAB_SIZE:32768
  NORMAL_TAB_SIZE:262015
  N_0:4
  [Support at least 262015 KVs <not include overflowTable>]
  ===========Config OK==========
  -----------Clear All----------
  |Insert| key:1,val:1
  |Insert| key:2,val:2
  |Insert| key:3,val:3
  |Insert| key:4,val:4
  |Insert| key:5,val:5
  |Insert| key:6,val:6
  |Insert| key:7,val:7
  |Insert| key:8,val:8
  |Insert| key:9,val:9
  |Insert| key:10,val:10
  |Insert| key:11,val:11
  |Insert| key:12,val:12
  |Insert| key:13,val:13
  |Insert| key:14,val:14
  |Insert| key:15,val:15
  |Insert| key:16,val:16
  |Insert| key:200,val:200
  |Insert| key:200,val:200 InsertErr
  ===========Table View=========
  Table:0 16,16|
  Table:1 1,1|
  Table:2 2,2|
  Table:3 3,3|
  Table:4 4,4|
  Table:5 5,5|
  Table:6 6,6|
  Table:7 7,7| -o- 15,15|
  Table:8 8,8| -o- 200,200|
  Table:9 9,9|
  Table:10 10,10|
  Table:11 11,11|
  Table:12 12,12|
  Table:13 13,13|
  Table:14 14,14|
  ===========Table OK===========
  |Search| key:1 result value:1
  |Search| key:2 result value:2
  |Search| key:3 result value:3
  |Search| key:4 result value:4
  |Search| key:5 result value:5
  |Search| key:6 result value:6
  |Search| key:7 result value:7
  |Search| key:8 result value:8
  |Search| key:9 result value:9
  |Search| key:10 result value:10
  |Search| key:11 result value:11
  |Search| key:12 result value:12
  |Search| key:13 result value:13
  |Search| key:14 result value:14
  |Search| key:15 result value:15
  |Search| key:16 result value:16
  |Update| key:1 update to 2
  |Update| key:2 update to 3
  |Update| key:3 update to 4
  |Update| key:4 update to 5
  |Update| key:5 update to 6
  |Update| key:6 update to 7
  |Update| key:7 update to 8
  |Update| key:8 update to 9
  |Update| key:9 update to 10
  |Update| key:10 update to 11
  |Update| key:11 update to 12
  |Update| key:12 update to 13
  |Update| key:13 update to 14
  |Update| key:14 update to 15
  |Update| key:15 update to 16
  |Update| key:16 update to 17
  ===========Table View=========
  Table:0 16,17|
  Table:1 1,2|
  Table:2 2,3|
  Table:3 3,4|
  Table:4 4,5|
  Table:5 5,6|
  Table:6 6,7|
  Table:7 7,8| -o- 15,16|
  Table:8 8,9| -o- 200,200|
  Table:9 9,10|
  Table:10 10,11|
  Table:11 11,12|
  Table:12 12,13|
  Table:13 13,14|
  Table:14 14,15|
  ===========Table OK===========
  |Remove| key:1 remove ok
  |Remove| key:2 remove ok
  |Remove| key:3 remove ok
  |Remove| key:4 remove ok
  |Remove| key:5 remove ok
  |Remove| key:6 remove ok
  |Remove| key:7 remove ok
  |Remove| key:8 remove ok
  |Remove| key:9 remove ok
  |Remove| key:10 remove ok
  |Remove| key:11 remove ok
  |Remove| key:12 remove ok
  |Remove| key:13 remove ok
  |Remove| key:14 remove ok
  |Remove| key:15 remove ok
  |Remove| key:16 remove ok
  ===========Table View=========
  Table:0 empty
  Table:1 empty
  Table:2 empty
  Table:3 empty
  Table:4 empty
  Table:5 empty
  Table:6 empty
  Table:7 empty
  Table:8 200,200|
  Table:9 empty
  Table:10 empty
  Table:11 empty
  Table:12 empty
  Table:13 empty
  Table:14 empty
  ===========Table OK===========
  ```
  </details> 

### GC
- 溢出表根据 `bitmap` 位示图进行分配回收
- `std::bitset<BITMAP_SIZE> bitmap` 位于pm中,以保证持久性
  <details>
  <summary>分配与回收</summary>

  ```c
  pm_table* PMLHash::newOverflowTable() {
    uint64_t offset = bitmap->findFirstAvailable();
    if (offset == BITMAP_SIZE) {
      // after throw this msg, the db may still can insert other kv, but we still
      // view it as full
      // we just throw error,instead of returning nullptr
      pmError.set(OfMemoryLimitErr);
      throw Error("newOverflowTable: memory size limit");
    }
    bitmap->set(offset);

    pm_table* table = (pm_table*)((pm_table*)overflow_addr + offset);
    table->init();
    meta->overflow_num++;
    return table;
  }


  // if this table have previousTable, you should set previousTable.next_offset
  int PMLHash::freeOverflowTable(uint64_t idx) {
    bitmap->reset(idx);
    meta->overflow_num--;
    pmem_persist(&(meta->overflow_num), sizeof(uint64_t));
    return 0;
  }
  ```
  </details>

### MultiThread
- 简单使用粗粒度的锁(每个基础操作[insert/search/update/remove]加锁)
- `std::shared_mutex rwMutx(C++17)` 位于普通内存(stack or heap)中,运行时易失变量,非持久
- 细粒度的锁, 甚至 lock-free
  - 经过多次尝试,目前看来我是难以在目前这份代码架构上做出真正的并发控制的,一方面是没学过,一方面是水平有限,只能将并行用锁改串行.
- 读写锁 raii-style
  ```c
  std::unique_lock wr_lock(rwMutx);
  std::shared_lock rd_lock(rwMutx);
  ```

### Doc
为每一个函数都添加了注释文档

## 单线程YCSB性能测试
起初采用了一边读一边insert/search的方法,然后对getline的循环测流逝时间.  
后来考虑到多线程性能测试的需要,因为多线程不可能让每个线程去并行读硬盘(总线总是串行的),所以改用了先把数据读进内存缓冲区,再insert/search(读/写)  
- 时间基本为纯的insert/search时间,不包括读磁盘
  <details>
  <summary>计时机制</summary>

  ```c
  auto t_start = std::chrono::high_resolution_clock::now();
  for (auto & key : keyBuf) {
    if (key.first == INSERT) {
      f->insert(key.second, key.second);
    } else {
      uint64_t value;
      assert(f->search(key.second, value) != -1);
      assert(value == key.second);
    }
  }
  auto t_end = std::chrono::high_resolution_clock::now();
  auto dur = std::chrono::duration<double, std::milli>(t_end - t_start).count();
  printf("run %s\tWTime: %fms \tOPS: %fM\tDelay: %fns\n", filePath.c_str(), dur,
         cnt / (dur * 1000), dur * 1000 / cnt);
  ```
  </details>
```yaml
=========YCSB Benchmark=======
注:时间基本为纯Insert/Search时间,不包括读文件(读磁盘) n_thread=1
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
设置并发线程数为4
```yaml
=========YCSB Benchmark=======
注:时间基本为纯Insert/Search时间,不包括读文件(读磁盘)   n_thread=4
load .././benchmark/10w-rw-0-100-load.txt       WTime: 102.260500ms     OPS: 0.977904M  Delay: 1.022595ns
run .././benchmark/10w-rw-0-100-load.txt        WTime: 83.131300ms      OPS: 1.202928M  Delay: 0.831305ns
load .././benchmark/10w-rw-25-75-load.txt       WTime: 101.272800ms     OPS: 0.987442M  Delay: 1.012718ns
run .././benchmark/10w-rw-25-75-load.txt        WTime: 92.044800ms      OPS: 1.086438M  Delay: 0.920439ns
load .././benchmark/10w-rw-50-50-load.txt       WTime: 101.576400ms     OPS: 0.984490M  Delay: 1.015754ns
run .././benchmark/10w-rw-50-50-load.txt        WTime: 90.916800ms      OPS: 1.099918M  Delay: 0.909159ns
load .././benchmark/10w-rw-75-25-load.txt       WTime: 101.140500ms     OPS: 0.988733M  Delay: 1.011395ns
run .././benchmark/10w-rw-75-25-load.txt        WTime: 81.201500ms      OPS: 1.231517M  Delay: 0.812007ns
load .././benchmark/10w-rw-100-0-load.txt       WTime: 101.009300ms     OPS: 0.990018M  Delay: 1.010083ns
run .././benchmark/10w-rw-100-0-load.txt        WTime: 83.637800ms      OPS: 1.195644M  Delay: 0.836370ns
==========Benchmark OK=========
```

## Comment
### About CRUD
设计合适抽象程度的高可复用基本函数  
以 `inner_search` 为例,无论是 `insert` , `remove` , 还是 `search` , `update` ,都依赖于内部的一个 `inner_search` 函数,但是他们又有不同的需求,比如 `remove` 要求不仅要找到元素,还要找到元素所在的页,以及前面的一页. 而其他功能则没有这样的需求.
- 在我的设计中, 使用了 `SearchEntryWithPage` 作为baseFunction,它拥有最多的参数:
  ```cpp
  pm_table* searchEnteyWithPage(const uint64_t& key,
                                uint64_t* pos = nullptr,
                                pm_table** previousTable = nullptr);
  ```
- `entry* pmLinHash::searchEntry(const uint64_t& key)`作为一个简单方便的api,内部则是调用了 `searchEntryWithPage`
  <details>
  <summary>SearchEntryWithPage</summary>

  ```cpp
  /**
   * @brief this will just be used in `remove()`
   * @param key
   * @param pos the position of the key in the table
   * @param previousTable the previous table of the return table
   * @return the table of the key
   */
  pm_table* pmLinHash::searchEnteyWithPage(const uint64_t& key,
                                        uint64_t* pos,
                                        pm_table** previousTable) {
    uint64_t idx = getRealHashIdx(key);
    pm_table* t = getNmTableFromIdx(idx);
    pm_table* pre = nullptr;
    while (1) {
      int pos_ = t->pos(key);
      if (pos_ >= 0) {
        if (pos != nullptr) {
          *pos = uint64_t(pos_);
        }
        if (previousTable != nullptr) {
          *previousTable = pre;
        }
        return t;
      }
      if (t->next_offset == NEXT_IS_NONE) {
        break;
      }
      pre = t;
      t = getOfTableFromIdx(t->next_offset);
    }
    return nullptr;
  }
  ```
  </details>
  <details>
  <summary>SearchEntry</summary>

  ```cpp
  /**
   * @brief searchEntry with key
   * @param key
   * @return return nullptr if not found
   */
  entry* pmLinHash::searchEntry(const uint64_t& key) {
    uint64_t pos;
    pm_table* t = searchEnteyWithPage(key, &pos);
    if (t == nullptr) {
      return nullptr;
    }
    return t->index(pos);
  }
  ```
  </details>

### About MultiThread
- 多线程由于有锁的竞态,导致其实效率很低下,这说明我的设计不太好(只是随意的添加了锁将并行操作改串行).  
  - 此外并发线程数的选择上,也不能太多,毕竟硬件线程有限.  
- 另一方面,这个性能测试的结果随时间差异很大,这可能与cpu的状态有关  
- 但无论如何,测试发现,单线程都要优于简单粗粒度加锁的多线程,这可能是因为数据集过少,导致的write操作过快的原因;  
  - 比较奇怪的是,多线程读写锁在100%读的情况下,也劣于单线程读,也许这就是网络库中要io复用而不是新开线程的原因
## Concurrency safe
> 这里不区分并发和并行  

多线程支持的两个目的
- 并发,时分复用,有效降低多用户系统的响应时间
- 并行,真正提高引擎读写性能

在事务处理中,一个数据库是并发安全的,如果一个并发操作的结果和它在串行执行时表现的一样,我们可以将增删查改这些基本操作视为一个事务  
在多事务中,一个事务必须满足
- 可重复读
- 不能读未提交数据
- 不能写未提交数据
### Fine-grained mutex
所谓细粒度的锁,可以认为就是同一时间会有多个线程在整个索引数据结构中
- 细粒度的锁(如果以桶为单位加锁)也许面临的问题
  - Insert
    - 其他线程插入导致的分裂,本线程刚好要插入被分裂的桶,进而插入了旧的桶,导致后续的search找不到元素
      - 这要求在插入前再次计算要插入的桶,并重新上锁
    - 分裂会导致对其他桶的访问,要上锁
    - 多个线程插入导致同时分裂导致的对其他桶的上锁
    - 多个线程插入导致同时分裂对meta->next,meta->level的原子修改等
  - Search
    - 简单只读
  - Update
    - 可以认为update是读而不是写,不用加写锁,只需在修改时使用 `CAS` 修改
      - 注意处理读到后被删除的情况,这要求我们检查key并修改value
        - 遗憾的是,似乎没有这样的api,典型的 `CAS` 是检查v并修改v
      - 一般的解决方法是延时删除,简单将对应删除位置1,保证临近的update仍然访问到这个键值.后续在定时gc中锁全表,真正删除键值
  - Remove
    - 也许可以延时删除
## Unsolved questions
- 如果a.cc文件需要引入一个头文件<unistd.h>,而a.h不需要,那么应该在a.cc还是a.h引入呢?
  - 目前的做法是在a.h引入,这样a.cc只需要引入"a.h"即可,比较干净
