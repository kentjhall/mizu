#pragma once

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/syscall.h>
#include <linux/mizu.h>
#include "core/hle/kernel/svc_results.h"

#define mizu_servctl(...)					\
({								\
    long __ret = syscall(__NR_mizu_servctl, __VA_ARGS__);	\
    if (__ret < 0) {						\
        errno = (u32)(-__ret); 					\
        __ret = -1;						\
    }								\
    __ret;							\
})

#define mizu_servctl_checked(cmd, ...)								\
({												\
    long __ret = mizu_servctl(cmd, __VA_ARGS__);						\
    ResultCode __rc(errno);									\
    if (__ret == -1 && __rc != Kernel::ResultSessionClosed) {					\
        LOG_CRITICAL(Service, #cmd " failed: {}", __rc.description.Value());			\
    }												\
    __ret;											\
})

#define mizu_servctl_write_buffer(to, from, size) \
	mizu_servctl_checked(MIZU_SCTL_WRITE_BUFFER, (long)(to), (long)(from), (long)(size))

#define mizu_servctl_read_buffer(from, to, size) \
	mizu_servctl_checked(MIZU_SCTL_READ_BUFFER, (long)(from), (long)(to), (long)(size))

#define mizu_servctl_map_memory(there, here, size) \
	mizu_servctl_checked(MIZU_SCTL_MAP_MEMORY, (long)(there), (long)(here), (long)(size))

#define mizu_servctl_write_buffer_to(to, from, size, pid)							\
({														\
    long __ret = mizu_servctl(MIZU_SCTL_WRITE_BUFFER_TO, (long)(to), (long)(from), (long)(size), (long)(pid));	\
    if (__ret == -1) {												\
        ResultCode __rc(errno);											\
	if (__rc == Kernel::ResultInvalidId) {									\
            LOG_WARNING(Service, "MIZU_SCTL_WRITE_BUFFER_TO failed, session closed?");				\
        } else {												\
            LOG_CRITICAL(Service, "MIZU_SCTL_WRITE_BUFFER_TO failed: {}", __rc.description.Value());		\
        }													\
    }														\
})

#define mizu_servctl_read_buffer_from(from, to, size, pid)							\
({														\
    long __ret = mizu_servctl(MIZU_SCTL_READ_BUFFER_FROM, (long)(from), (long)(to), (long)(size), (long)(pid));	\
    if (__ret == -1) {												\
        ResultCode __rc(errno);											\
	if (__rc == Kernel::ResultInvalidId) {									\
            LOG_WARNING(Service, "MIZU_SCTL_READ_BUFFER_FROM failed, session closed?");				\
        } else {												\
            LOG_CRITICAL(Service, "MIZU_SCTL_READ_BUFFER_FROM failed: {}", __rc.description.Value());		\
        }													\
    }														\
    __ret;													\
})
