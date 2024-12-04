#pragma once

#include <helib/c.h>

C_FUNC pubkey_from_seckey(void **pubkey, void *seckey);

C_FUNC pubkey_destroy(void *pubkey);

C_FUNC pubkey_encrypt(void **ctxt, void *pubkey, void *ptxt_ZZ);

C_FUNC pubkey_packed_encrypt(void **ctxt, void *pubkey, void *ptxt_ZZX);
