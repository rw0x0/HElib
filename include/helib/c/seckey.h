#include "helib/c/c.h"

C_FUNC seckey_build(void **seckey, void *context);

C_FUNC seckey_destroy(void *seckey);

C_FUNC seckey_encrypt(void **ctxt, void *seckey, void *ptxt_ZZ);
