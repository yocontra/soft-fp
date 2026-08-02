#include "platform.h"
#include "softfloat.h"

uint_fast8_t sf128_backend_get_rounding(void) { return softfloat_roundingMode; }
void sf128_backend_set_rounding(uint_fast8_t value) { softfloat_roundingMode = value; }
uint_fast8_t sf128_backend_get_flags(void) { return softfloat_exceptionFlags; }
void sf128_backend_set_flags(uint_fast8_t value) { softfloat_exceptionFlags = value; }
