#ifndef UTIL
#define UTIL
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

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

#endif