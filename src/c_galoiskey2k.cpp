#include <helib/c_galoiskey2k.h>
#include <helib/GaloisKey2k.h>

C_FUNC GK_build(void **gk, long m) {
    IfNullRet(gk, E_POINTER);
    *gk = new helib::GaloisKey2k(m);
    return S_OK;
}

C_FUNC GK_destroy(void *gk) {
    helib::GaloisKey2k *gk_ = FromVoid<helib::GaloisKey2k>(gk);
    IfNullRet(gk_, E_POINTER);
    delete gk_;
    return S_OK;
}

C_FUNC GK_generate_step(void *gk, void *seckey, int step) {
    helib::GaloisKey2k *gk_ = FromVoid<helib::GaloisKey2k>(gk);
    IfNullRet(gk_, E_POINTER);
    helib::SecKey *seckey_ = FromVoid<helib::SecKey>(seckey);
    IfNullRet(seckey_, E_POINTER);
    gk_->generate_step(*seckey_, step);
    return S_OK;
}

C_FUNC GK_rotate(void *gk, void *ctxt, int step) {
    helib::GaloisKey2k *gk_ = FromVoid<helib::GaloisKey2k>(gk);
    IfNullRet(gk_, E_POINTER);
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    gk_->rotate(*ctxt_, step);
    return S_OK;
}
