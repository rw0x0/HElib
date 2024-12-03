#include <helib/c/c.h>
#include <helib/helib.h>

C_FUNC seckey_build(void **seckey, void *context) {
    helib::Context *context_ = FromVoid<helib::Context>(context);
    IfNullRet(context_, E_POINTER);
    IfNullRet(seckey, E_POINTER);

    *seckey = new helib::SecKey(*context_);
    helib::SecKey *seckey_ = FromVoid<helib::SecKey>(*seckey);
    IfNullRet(seckey_, E_POINTER);
    seckey_->GenSecKey();
    return S_OK;
}

C_FUNC seckey_destroy(void *seckey) {
    helib::SecKey *seckey_ = FromVoid<helib::SecKey>(seckey);
    IfNullRet(seckey_, E_POINTER);
    delete seckey_;
    return S_OK;
}

C_FUNC seckey_encrypt(void **ctxt, void *seckey, void *ptxt_ZZ) {
    helib::SecKey *seckey_ = FromVoid<helib::SecKey>(seckey);
    IfNullRet(seckey_, E_POINTER);
    IfNullRet(ctxt, E_POINTER);

    NTL::ZZ *ptxt_ZZ_ = FromVoid<NTL::ZZ>(ptxt_ZZ);
    IfNullRet(ptxt_ZZ_, E_POINTER);

    *ctxt = new helib::Ctxt(*seckey_);
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(*ctxt);
    IfNullRet(ctxt_, E_POINTER);

    NTL::ZZX ptxt = NTL::ZZX(*ptxt_ZZ_);
    seckey_->Encrypt(*ctxt_, ptxt);
    return S_OK;
}
