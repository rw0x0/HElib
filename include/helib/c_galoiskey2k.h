#pragma once

#include <helib/c.h>

C_FUNC GK_build(void **gk, long m);

C_FUNC GK_destroy(void *gk);

C_FUNC GK_generate_step(void *gk, void *seckey, int step);

C_FUNC GK_rotate(void *gk, void *ctxt, int step);
