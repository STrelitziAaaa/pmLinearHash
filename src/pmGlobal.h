/**
 * * Created on 2020/12/19
 * * Editor: lwm 
 * * This file is to define global macros, i.e. some constexpr
 */


#ifndef __PMGLOBAL_H__
#define __PMGLOBAL_H__

#define TABLE_SIZE 1                                // adjustable
#define HASH_SIZE 16                                // adjustable
#define FILE_SIZE 1024 * 1024 * 16                  // 16 MB adjustable
#define BITMAP_SIZE 1024 * 1024 / sizeof(pm_table)  // for gc of overflowT
// #define BITMAP_SIZE 128  // for gc of overflow table
#define NORMAL_TAB_SIZE \
  (((FILE_SIZE / 2) - sizeof(bitmap_st) - sizeof(metadata)) / sizeof(pm_table))
#define OVERFLOW_TABL_SIZE BITMAP_SIZE
#define N_0 4
#define N_LEVEL(level) (pow(2, level) * N_0)

// typically we will use -1,but unfortunately the dataType is uint64_t
#define NEXT_IS_NONE  999999UL
#define VALUE_NOT_FOUND NEXT_IS_NONE


#endif // __PMGLOBAL_H__