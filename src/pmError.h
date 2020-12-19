/**
 * * Created on 2020/12/19
 * * Editor: lwm 
 * * this file is to declare global error, it will be defined using thread_local
 */


#ifndef __PMERROR_H__
#define __PMERROR_H__

#include <iostream>
#include <string>

typedef enum pm_errno {
  Success = 0,
  DupKeyErr,
  NmMemoryLimitErr,
  OfMemoryLimitErr,
  NotFoundErr,
} pm_errno;

class pm_err {
 private:
  pm_errno errNo_;

 public:
  pm_err() : errNo_(Success) {}
  void set(pm_errno errNo) { errNo_ = errNo; }
  void perror(std::string str) {
    std::cout << str << " :" << string() << std::endl;
  }
  std::string string() {
    switch (errNo_) {
      case Success:
        return "success";
      case DupKeyErr:
        return "duplicate key";
      case NmMemoryLimitErr:
        return "allocate normal table:  memory size limit";
      case OfMemoryLimitErr:
        return "allocate overflow table:  memory size limit";
      case NotFoundErr:
        return "key not found";
      default:
        return "unkownn error";
    }
  }
};
#endif  // __PMERROR_H__