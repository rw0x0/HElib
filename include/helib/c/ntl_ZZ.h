#include <helib/c/c.h>

C_FUNC ZZ_from_string(void **ZZ, const char *s);

C_FUNC ZZ_from_long(void **ZZ, long a);

C_FUNC ZZ_destroy(void *ZZ);

C_FUNC ZZ_from_bytes(void **ZZ, const unsigned char *buf, long len);

C_FUNC ZZ_to_bytes(void *ZZ, unsigned char **buf, long len);

C_FUNC ZZ_bytes(void *ZZ, long *len);

C_FUNC ZZ_random(void **ZZ, void *mod_ZZ);
