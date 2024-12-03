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

C_FUNC ZZ_from_bytes(void **ZZ, const unsigned char *buf, long len) {
    IfNullRet(ZZ, E_POINTER);
    IfNullRet(buf, E_POINTER);
    *ZZ = new NTL::ZZ;
    NTL::ZZ *ZZ_ = FromVoid<NTL::ZZ>(ZZ);
    IfNullRet(ZZ_, E_POINTER);
    NTL::ZZFromBytes(*ZZ_, buf, len);
    return S_OK;
}

C_FUNC ZZ_to_bytes(void *ZZ, unsigned char *buf, long len) {
    NTL::ZZ *ZZ_ = FromVoid<NTL::ZZ>(ZZ);
    IfNullRet(ZZ_, E_POINTER);
    IfNullRet(buf, E_POINTER);

    NTL::BytesFromZZ(buf, *ZZ_, len);
    return S_OK;
}

C_FUNC ZZ_bytes(void *ZZ, long *len) {
    NTL::ZZ *ZZ_ = FromVoid<NTL::ZZ>(ZZ);
    IfNullRet(ZZ_, E_POINTER);
    IfNullRet(len, E_POINTER);

    *len = NumBytes(*ZZ_);
    return S_OK;
}

C_FUNC ZZ_random(void **ZZ, void *mod_ZZ) {
    IfNullRet(ZZ, E_POINTER);
    *ZZ = new NTL::ZZ;
    NTL::ZZ *ZZ_ = FromVoid<NTL::ZZ>(ZZ);
    IfNullRet(ZZ_, E_POINTER);
    NTL::ZZ *mod_ZZ_ = FromVoid<NTL::ZZ>(mod_ZZ);
    IfNullRet(mod_ZZ_, E_POINTER);
    NTL::RandomBnd(*ZZ_, *mod_ZZ_);
    return S_OK;
}
