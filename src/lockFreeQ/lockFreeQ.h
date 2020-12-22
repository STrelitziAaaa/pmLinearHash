#ifndef __LOCKFREEQ_H__
#define __LOCKFREEQ_H__

// #include <atomic>
#include <thread>

using namespace std;

// #define cas(addr, oldval, newval) \
//   atomic_compare_exchange_weak(&addr, &oldval, newval)

#define cas(a, b, c) __sync_bool_compare_and_swap(a, b, c)

template <typename T>
class node {
 private:
 public:
  node* next;
  T data;
  node() : next(nullptr), data(T{}) {
    // bzero(&data, sizeof(data));
  }
  node(const T& data) : next(nullptr), data(data) {}
};

template <typename T>
class syncQueue {
 private:
  static constexpr bool Q_NONBLOCK = false;
  static constexpr bool Q_BLOCK = true;

  node<T>* head;
  node<T>* tail;
  // atomic<int32_t> size;

  bool enQueue(const T& data, bool is_block) {
    node<T>* new_node = new node<T>(data);
    // fetch snapshot
    node<T>* tail_ss;
    node<T>* next_ss;
    while (1) {
      tail_ss = tail;
      next_ss = tail_ss->next;

      if (tail_ss != tail) {
        if (!is_block) {
          return false;
        }
        continue;
      }
      // help occupying thread move tail
      if (next_ss != nullptr) {
        cas(&tail, tail_ss, next_ss);
        // tail.compare_exchange_weak(tail_ss, next_ss);
        continue;
      }
      if (cas(&(tail_ss->next), next_ss, new_node)) {
        break;
      }
    }
    // move tail
    cas(&tail, tail_ss, new_node);
    return true;
  }

  bool deQueue(T& data, bool is_block) {
    node<T>* head_ss;
    node<T>* tail_ss;
    node<T>* next_ss;

    while (1) {
      head_ss = head;
      tail_ss = tail;
      next_ss = head_ss->next;

      if (head_ss != head) {
        continue;
      }

      // if empty queue
      if (head_ss == tail_ss && !next_ss) {
        if (!is_block) {
          return false;
        }
        continue;
      }
      // if tail not sync, help it
      if (head_ss == tail_ss && next_ss) {
        cas(&tail, tail_ss, next_ss);
        continue;
      }
      // dequeue
      if (cas(&head, head_ss, next_ss)) {
        data = next_ss->data;
        delete head_ss;
        break;
      }
    }
    return true;
  }

 public:
  syncQueue() : head(new node<T>()), tail(head) {}

  // used to be compatible with boost::lockfree::queue
  syncQueue(int ignore) : head(new node<T>()), tail(head) {}

  syncQueue& operator<<(const T& data) {
    enQueue(data, Q_BLOCK);
    return *this;
  }
  syncQueue& operator>>(T& data) {
    dnQueue(data, Q_BLOCK);
    return *this;
  }

  // it is blocked
  bool push(const T& data) { return enQueue(data, Q_NONBLOCK); }

  // it is blocked
  bool pop(T& data) { return deQueue(data, Q_NONBLOCK); }
};

#endif  // __LOCKFREEQ_H__