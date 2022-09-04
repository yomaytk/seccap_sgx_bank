#ifndef _USER_TYPES_H_
#define _USER_TYPES_H_

/* User defined types */

#define LOOPS_PER_THREAD 500

enum ProcessResult {
    PROCESS_SUCCESS,
    DEPOSIT_OVERFLOW,
    NEED_LOGOUT,
    ACCOUNT_SET_FAIL,
};

typedef void* buffer_t;
typedef int array_t[10];

#endif