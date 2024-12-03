#include <helib/c/c.h>
#include <NTL/ZZ.h>
#include <NTL/tools.h>

C_FUNC ZZ_from_string(void **ZZ, const char *s) {
    IfNullRet(ZZ, E_POINTER);
    *ZZ = new NTL::ZZ(NTL::INIT_VAL, s);
    return S_OK;
}

C_FUNC ZZ_from_long(void **ZZ, long a) {
    IfNullRet(ZZ, E_POINTER);
    *ZZ = new NTL::ZZ(NTL::INIT_VAL, a);
    return S_OK;
}

C_FUNC ZZ_destroy(void *ZZ) {
    NTL::ZZ *ZZ_ = FromVoid<NTL::ZZ>(ZZ);
    IfNullRet(ZZ_, E_POINTER);
    delete ZZ_;
    return S_OK;
}
