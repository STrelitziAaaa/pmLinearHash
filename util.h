#ifndef UTIL_H
#define UTIL_H
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string>

// check and create file
bool chkAndCrtFile(const char* filePath) {
  struct stat fstate;
  if (!stat(filePath, &fstate)) {
    printf("file exists\n");
    return true;
  } else {
    if (errno == ENOENT) {
      // note: the file mode may be set as 0644 even if we give 0666
      // todo: here we need to mkdir
      close(creat(filePath, 0666));
      printf("file created\n");
      return false;
    } else {
      perror("stat");
    }
  }
}

class Error {
 private:
  std::string msg;

 public:
  Error(std::string msg) : msg(msg) {}
  Error(const char* msg) : msg(msg) {}
  std::string string() { return msg; };
};

#define raise(...)                                        \
  do {                                                    \
    printf(                                               \
        "%s[line %d] %s(): "                              \
        "%s"                                              \
        "\n",                                             \
        __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
    assert(0);                                            \
  } while (0);

#endif