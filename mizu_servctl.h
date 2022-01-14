#pragma once

#include <unistd.h>
#include <arpa/inet.h>

#define __NR_mizu_servctl 501

enum {
    MIZU_SCTL_REGISTER_NAMED_SERVICE = 0,
    MIZU_SCTL_GET_CMD,
    MIZU_SCTL_PUT_CMD,
    MIZU_SCTL_CREATE_SESSION_HANDLE,
    MIZU_SCTL_CREATE_COPY_HANDLE,
    MIZU_SCTL_GET_PROCESS_ID,
    MIZU_SCTL_WRITE_BUFFER,
    MIZU_SCTL_READ_BUFFER,
    MIZU_SCTL_WRITE_BUFFER_TO,
    MIZU_SCTL_READ_BUFFER_FROM,
};

#define mizu_servctl(...)				\
({							\
    long ret = syscall(__NR_mizu_servctl, __VA_ARGS__);	\
    if (ret < 0) {					\
        errno = (u32)(-ret); 				\
        ret = -1;					\
    }							\
    ret;						\
})
