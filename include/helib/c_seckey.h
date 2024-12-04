#pragma once

#include <helib/c.h>

C_FUNC seckey_build(void **seckey, void *context);

C_FUNC seckey_destroy(void *seckey);

C_FUNC seckey_encrypt(void **ctxt, void *seckey, void *ptxt_ZZ);

C_FUNC seckey_packed_encrypt(void **ctxt, void *seckey, void *ptxt_ZZX);

C_FUNC seckey_decrypt(void **ptxt_ZZ, void *seckey, void *ctxt);

C_FUNC seckey_packed_decrypt(void **ptxt_ZZX, void *seckey, void *ctxt);
