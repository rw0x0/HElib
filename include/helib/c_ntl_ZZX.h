#pragma once

#include <helib/c.h>

C_FUNC ZZX_from_len(void **ZZX, long len);

C_FUNC ZZX_destroy(void *ZZX);

C_FUNC ZZX_set_index(void *ZZX, long index, void *ZZ);

C_FUNC ZZX_get_index(void **ZZ, void *ZZX, long index);

C_FUNC ZZX_get_length(void *ZZX, long *len);
