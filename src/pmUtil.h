#ifndef __PMUTIL_H__
#define __PMUTIL_H__

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <functional>

// you must use static or define a class declaring friend,
// otherwise the compiler will give a redifinition error
// if defined static,you will have warning of nonused-func
static int chkAndCrtFile(const char* filePath);
inline void assertOk(int x);
inline void assertFail(int x);

class ignore {
 public:
  /**
   * @brief check and create file
   * @param filePath
   * @return return -1 if create error; 0 if success
   */
  friend int chkAndCrtFile(const char* filePath) {
    struct stat fstate;
    if (!stat(filePath, &fstate)) {
      printf("pmFile Exists:Recover\n");
      return -1;
    } else {
      if (errno == ENOENT) {
        // note: the file mode may be set as 0644 even if we give 0666
        // todo: here we need to mkdir
        close(creat(filePath, 0666));
        printf("pmFile Created:Init\n");
        return 0;
      }
      perror("stat");
      exit(EXIT_FAILURE);
    }
    return -1;
  }
  friend inline void assertOk(int x) { assert(x != -1); }
  friend inline void assertFail(int x) { assert(x == -1); }
};

class scope_guard {
 private:
  using callbackF = std::function<void(void)>;
  callbackF f;

 public:
  // recommended: you should pass lambda func with closure
  scope_guard(callbackF&& f) : f(f) {}
  ~scope_guard() { f(); }
};

using defer = scope_guard;

#endif  // __PMUTIL_H__