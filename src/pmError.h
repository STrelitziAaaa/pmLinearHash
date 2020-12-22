/**
 * * Created on 2020/12/19
 * * Editor: lwm
 * * this file is to declare global error, it will be defined using thread_local
 */

#ifndef __PMERROR_H__
#define __PMERROR_H__

#include <iostream>
#include <mutex>
#include <string>

class pmError;
extern std::mutex pmErrorPrintMutx;
extern thread_local pmError pmError_tls;

typedef enum pmErrno {
  Err_Success = 0,
  Err_DupKeyErr,
  Err_NmMemoryLimitErr,
  Err_OfMemoryLimitErr,
  Err_NotFoundErr,
} pmErrno;

class pmError {
 private:
  pmErrno errNo_;

 public:
  pmError() : errNo_(Err_Success) {}
  pmError(const pmError& rhs) { errNo_ = rhs.errNo_; }
  pmError& operator=(const pmError& rhs) { errNo_ = rhs.errNo_; }
  void set(pmErrno errNo) { errNo_ = errNo; }
  void set(const pmError& rhs) { errNo_ = rhs.errNo_; }
  void success() { errNo_ = Err_Success; }
  void perror(std::string str) {
    std::cout << str << " :" << string() << std::endl;
  }
  bool hasError() { return errNo_ == Err_Success ? 0 : 1; }
  const char* c_str() const {
    switch (errNo_) {
      case Err_Success:
        return "success";
      case Err_DupKeyErr:
        return "duplicate key";
      case Err_NmMemoryLimitErr:
        return "allocate normal table:  memory size limit";
      case Err_OfMemoryLimitErr:
        return "allocate overflow table:  memory size limit";
      case Err_NotFoundErr:
        return "key not found";
      default:
        return "unkownn error";
    }
  }

  const std::string string() const { return std::string(c_str()); }

  void println(std::string prefix = "") const {
    std::lock_guard<std::mutex> lock(pmErrorPrintMutx);
    printf("%s pmerror:%s \n", prefix.c_str(), c_str());
  }
};

#endif  // __PMERROR_H__