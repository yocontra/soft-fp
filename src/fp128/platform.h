#pragma once

/* Production platform contract for Berkeley SoftFloat. */
#if defined(_WIN32) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define LITTLEENDIAN 1
#endif

#if defined(_MSC_VER)
#define INLINE __inline
#elif defined(__GNUC_STDC_INLINE__)
#define INLINE inline
#else
#define INLINE extern inline
#endif

#ifndef THREAD_LOCAL
#if defined(__cplusplus)
#define THREAD_LOCAL thread_local
#elif defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL _Thread_local
#endif
#endif
