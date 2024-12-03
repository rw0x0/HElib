#include "helib/c/c.h"

C_FUNC pubkey_build(void **seckey, void *context);

C_FUNC pubkey_destroy(void *seckey);

C_FUNC pubkey_from_seckey(void **pubkey, void *seckey);