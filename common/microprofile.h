// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

// Uncomment this to disable microprofile. This will get you cleaner profiles when using
// external sampling profilers like "Very Sleepy", and will improve performance somewhat.
// #define MICROPROFILE_ENABLED 0

// Customized Citra settings.
// This file wraps the MicroProfile header so that these are consistent everywhere.
#define MICROPROFILE_WEBSERVER 0
#define MICROPROFILE_GPU_TIMERS 0 // TODO: Implement timer queries when we upgrade to OpenGL 3.3
#define MICROPROFILE_CONTEXT_SWITCH_TRACE 0
#define MICROPROFILE_PER_THREAD_BUFFER_SIZE (2048 << 13) // 16 MB

#ifdef _WIN32
// This isn't defined by the standard library in MSVC2015
typedef void* HANDLE;
#endif

#include <microprofile.h>

#define MP_RGB(r, g, b) ((r) << 16 | (g) << 8 | (b) << 0)

// On OS X, some Mach header included by MicroProfile defines these as macros, conflicting with
// identifiers we use.
#ifdef PAGE_SIZE
#undef PAGE_SIZE
#endif
#ifdef PAGE_MASK
#undef PAGE_MASK
#endif
