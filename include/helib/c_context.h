#pragma once

#include <helib/c.h>

C_FUNC context_build(void **context, long m, void *p, long bits);

C_FUNC context_destroy(void *context);

C_FUNC context_printout(void *context);
