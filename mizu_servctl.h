#pragma once

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/syscall.h>
#include <linux/horizon.h>
#include "core/hle/kernel/svc_results.h"

#define mizu_servctl(...)					\
({								\
    long __ret = syscall(__NR_horizon_servctl, __VA_ARGS__);	\
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
	mizu_servctl_checked(HZN_SCTL_WRITE_BUFFER, (long)(to), (long)(from), (long)(size))

#define mizu_servctl_read_buffer(from, to, size) \
	mizu_servctl_checked(HZN_SCTL_READ_BUFFER, (long)(from), (long)(to), (long)(size))

#define mizu_servctl_map_memory(there, here, size) \
	mizu_servctl_checked(HZN_SCTL_MAP_MEMORY, (long)(there), (long)(here), (long)(size))

#define mizu_servctl_write_buffer_to(to, from, size, pid)							\
({														\
    long __ret = mizu_servctl(HZN_SCTL_WRITE_BUFFER_TO, (long)(to), (long)(from), (long)(size), (long)(pid));	\
    if (__ret == -1) {												\
        ResultCode __rc(errno);											\
	if (__rc == Kernel::ResultInvalidId) {									\
            LOG_WARNING(Service, "HZN_SCTL_WRITE_BUFFER_TO failed, session closed?");				\
        } else {												\
            LOG_CRITICAL(Service, "HZN_SCTL_WRITE_BUFFER_TO failed: {}", __rc.description.Value());		\
        }													\
    }														\
})

#define mizu_servctl_read_buffer_from(from, to, size, pid)							\
({														\
    long __ret = mizu_servctl(HZN_SCTL_READ_BUFFER_FROM, (long)(from), (long)(to), (long)(size), (long)(pid));	\
    if (__ret == -1) {												\
        ResultCode __rc(errno);											\
	if (__rc == Kernel::ResultInvalidId) {									\
            LOG_WARNING(Service, "HZN_SCTL_READ_BUFFER_FROM failed, session closed?");				\
        } else {												\
            LOG_CRITICAL(Service, "HZN_SCTL_READ_BUFFER_FROM failed: {}", __rc.description.Value());		\
        }													\
    }														\
    __ret;													\
})

#define mizu_servctl_memwatch(cmd, pid, addr, size, vec, vec_len)						\
({														\
    long __ret = mizu_servctl(cmd, (long)(pid), (long)(addr), (long)(size), (long)(vec), (long)(vec_len));	\
    if (__ret == -1) {												\
        ResultCode __rc(errno);											\
	if (__rc == Kernel::ResultInvalidId) {									\
            LOG_WARNING(Service, #cmd " failed, session closed?");						\
        } else {												\
            LOG_CRITICAL(Service, #cmd " failed failed: {}", __rc.description.Value());				\
        }													\
    }														\
    __ret;													\
})

#define mizu_servctl_memwatch_get(pid, addr, size, vec, vec_len) \
	mizu_servctl_memwatch(HZN_SCTL_MEMWATCH_GET, pid, addr, size, vec, vec_len)

#define mizu_servctl_memwatch_get_clear(pid, addr, size, vec, vec_len) \
	mizu_servctl_memwatch(HZN_SCTL_MEMWATCH_GET_CLEAR, pid, addr, size, vec, vec_len)
