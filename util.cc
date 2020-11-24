#include "util.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include "pml_hash.h"

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

constexpr uint64_t INSERT = 1;
constexpr uint64_t READ = 2;
map<std::string, uint64_t> TypeDict = {{"INSERT", INSERT}, {"READ", READ}};

int runYCSBBenchmark(const char* filePath, PMLHash* f) {
  std::ifstream in_(filePath);
  std::string typeStr;
  uint64_t key;
  double total;

  auto t_start = std::chrono::high_resolution_clock::now();
  while (!in_.eof()) {
    in_ >> typeStr >> key;
    if (typeStr == "INSERT") {
      f->insert(key, key);

    } else {
      uint64_t value;
      f->search(key, value);
    }
  }
  auto t_end = std::chrono::high_resolution_clock::now();
  std::cout
      << "run " << filePath << " | WallTime: "
      << std::chrono::duration<double, std::milli>(t_end - t_start).count()
      << " ms" << std::endl;
  return 0;
}

int loadYCSBBenchmark(const char* filePath, PMLHash* f) {
  std::ifstream in_(filePath);
  std::string typeStr;
  uint64_t key;
  double total;
  auto t_start = std::chrono::high_resolution_clock::now();
  while (!in_.eof()) {
    in_ >> typeStr >> key;
    // std::cout << "insert " << key << std::endl;
    f->insert(key, key);
  }
  auto t_end = std::chrono::high_resolution_clock::now();
  std::cout
      << "load " << filePath << " | WallTime: "
      << std::chrono::duration<double, std::milli>(t_end - t_start).count()
      << " ms" << std::endl;
  return 0;
}