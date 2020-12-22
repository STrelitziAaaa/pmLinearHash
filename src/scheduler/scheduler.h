#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <atomic>
#include <boost/lockfree/queue.hpp>
#include "../lockFreeQ/lockFreeQ.h"
#include "../pmError.h"
#include "../pmLinHash.h"
#include "../pmUtil.h"
#include "semaphore.h"

#ifdef PMDEBUG
#undef PMDEBUG
#endif

enum TASK_OP { OP_SEARCH = 1, OP_INSERT, OP_UPDATE, OP_REMOVE, OP_INIT };

// todo: update c-style semaphore to cpp-style
struct taskItem {
  TASK_OP op;  // read or write
  uint64_t key;
  uint64_t* value;
  sem_t* preparing;
  pmError* error;

  taskItem()
      : op(OP_INIT),
        key(0),
        value(nullptr),
        preparing(nullptr),
        error(nullptr) {}
  taskItem(const taskItem& rhs)
      : op(rhs.op),
        key(rhs.key),
        value(rhs.value),
        preparing(rhs.preparing),
        error(rhs.error) {}
  taskItem(TASK_OP op, const uint64_t& key, uint64_t* value)
      : op(op), key(key), value(value) {
    preparing = (sem_t*)malloc(sizeof(sem_t));
    sem_init(preparing, 0, 0);
    error = new pmError();
  }
  void setError(const pmError& rhs) { *error = rhs; }
  const pmError& getError() { return *error; }
  void wait() { sem_wait(preparing); };
  void signal() { sem_post(preparing); };

  // ! note: u must never define destruct function
  int destroy() {
    free(preparing);
    delete (error);
  }

 private:
};

class scheduler {
 private:
  syncQueue<taskItem> chan;
  // boost::lockfree::queue<taskItem, boost::lockfree::fixed_sized<false> > chan;
  pmLinHash* f;
  atomic<int> notDone;

 public:
  scheduler(pmLinHash* f, int n_thread)
      : f(f), chan(10000), notDone(n_thread) {}
  int RunAndServe() {
    while (1) {
      taskItem item;
      while (!chan.pop(item)) {
        if (!notDone.load()) {
          return 0;
        }
      };
      switch (item.op) {
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
  int Do(TASK_OP op, const uint64_t& key, uint64_t* value) {
    taskItem item(op, key, value);
    // todo: fix it!
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

  int Done() { notDone--; }
};

// class scheduler {
//  private:
//   syncQueue<taskItem> chan;
//   pmLinHash* f;

//  public:
//   scheduler(pmLinHash* f) : f(f), chan() {}
//   int RunAndServe() {
//     while (1) {
//       taskItem item;
//       chan >> item;
//       switch (item.op) {
//         case OP_INSERT:
//           f->insert(item.key, *(item.value));
//           break;
//         case OP_REMOVE:
//           f->remove(item.key);
//           break;
//         case OP_SEARCH:
//           f->search(item.key, item.value);
//           break;
//         case OP_UPDATE:
//           f->update(item.key, *(item.value));
//           break;
//       }
//       sem_post(item.preparing);
//       printf("[RECV] op:%d key:%d\n", item.op, item.key);
//     }
//   }
//   int Do(TASK_OP op, const uint64_t& key, uint64_t* value) {
//     taskItem item(op, key, value);
//     chan << item;
//     sem_wait(item.preparing);
//     printf("\t[wake] key:%d\n", key);
//     item.destroy();
//   }
// };

#endif  // __SCHEDULER_H__