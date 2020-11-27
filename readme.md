
# pmLinearHash
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
  ```c
  PMLHash* f = new PMLHash("./pmemFile");
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
- 后来考虑到多线程性能测试的需要,因为多线程不可能让每个线程去并行读硬盘(总线是串行的),所以后来改用了先把数据读进内存缓冲区,再insert/search
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

