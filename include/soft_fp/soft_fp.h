#pragma once

/* Unified public C/C++ API for all enabled soft-fp formats. */

#include "soft_fp64/config.h"
#include "soft_fp64/soft_fp64.h"

#if SOFT_FP_HAS_FP128
#include "soft_fp128/soft_f128.h"
#endif

#if SOFT_FP_HAS_FP256
#include "soft_fp256/soft_f256.h"
#endif
