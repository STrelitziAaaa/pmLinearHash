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
      close(creat(filePath, 0666));
      printf("file created\n");
      // todo: here we need to mkdir
      return false;
    } else {
      perror("stat");
    }
  }
}

#endif