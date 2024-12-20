#include <helib/c_seckey.h>
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

C_FUNC seckey_packed_encrypt(void **ctxt, void *seckey, void *ptxt_ZZX) {
    helib::SecKey *seckey_ = FromVoid<helib::SecKey>(seckey);
    IfNullRet(seckey_, E_POINTER);
    IfNullRet(ctxt, E_POINTER);

    NTL::ZZX *ptxt_ZZX_ = FromVoid<NTL::ZZX>(ptxt_ZZX);
    IfNullRet(ptxt_ZZX_, E_POINTER);

    *ctxt = new helib::Ctxt(*seckey_);
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(*ctxt);
    IfNullRet(ctxt_, E_POINTER);

    seckey_->Encrypt(*ctxt_, *ptxt_ZZX_);
    return S_OK;
}

C_FUNC seckey_decrypt(void **ptxt_ZZ, void *seckey, void *ctxt) {
    helib::SecKey *seckey_ = FromVoid<helib::SecKey>(seckey);
    IfNullRet(seckey_, E_POINTER);
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);

    IfNullRet(ptxt_ZZ, E_POINTER);
    *ptxt_ZZ = new NTL::ZZ;
    NTL::ZZ *ZZ = FromVoid<NTL::ZZ>(*ptxt_ZZ);
    IfNullRet(ZZ, E_POINTER);

    NTL::ZZX decrypted;
    seckey_->Decrypt(decrypted, *ctxt_);
    *ZZ = decrypted[0];
    return S_OK;
}

C_FUNC seckey_packed_decrypt(void **ptxt_ZZX, void *seckey, void *ctxt) {
    helib::SecKey *seckey_ = FromVoid<helib::SecKey>(seckey);
    IfNullRet(seckey_, E_POINTER);
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);

    IfNullRet(ptxt_ZZX, E_POINTER);
    *ptxt_ZZX = new NTL::ZZX;
    NTL::ZZX *ZZX = FromVoid<NTL::ZZX>(*ptxt_ZZX);
    IfNullRet(ZZX, E_POINTER);

    seckey_->Decrypt(*ZZX, *ctxt_);
    return S_OK;
}
