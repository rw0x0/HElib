#include <helib/c_ntl_ZZX.h>
#include <NTL/ZZX.h>

C_FUNC ZZX_from_len(void **ZZX, long len) {
    IfNullRet(ZZX, E_POINTER);
    *ZZX = new NTL::ZZX();
    NTL::ZZX *ZZX_ = FromVoid<NTL::ZZX>(*ZZX);
    IfNullRet(ZZX_, E_POINTER);
    ZZX_->SetLength(len);
    return S_OK;
}

C_FUNC ZZX_destroy(void *ZZX) {
    NTL::ZZX *ZZX_ = FromVoid<NTL::ZZX>(ZZX);
    IfNullRet(ZZX_, E_POINTER);
    delete ZZX_;
    return S_OK;
}

C_FUNC ZZX_set_index(void *ZZX, long index, void *ZZ) {
    NTL::ZZX *ZZX_ = FromVoid<NTL::ZZX>(ZZX);
    IfNullRet(ZZX_, E_POINTER);
    NTL::ZZ *ZZ_ = FromVoid<NTL::ZZ>(ZZ);
    IfNullRet(ZZ_, E_POINTER);
    (*ZZX_)[index] = *ZZ_;
    return S_OK;
}

C_FUNC ZZX_get_index(void **ZZ, void *ZZX, long index) {
    IfNullRet(ZZ, E_POINTER);
    NTL::ZZX *ZZX_ = FromVoid<NTL::ZZX>(ZZX);
    IfNullRet(ZZX_, E_POINTER);
    *ZZ = new NTL::ZZ((*ZZX_)[index]);
    return S_OK;
}

C_FUNC ZZX_get_length(void *ZZX, long *len) {
    NTL::ZZX *ZZX_ = FromVoid<NTL::ZZX>(ZZX);
    IfNullRet(ZZX_, E_POINTER);
    IfNullRet(len, E_POINTER);
    *len = ZZX_->rep.length();
    return S_OK;
}
