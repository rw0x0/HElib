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
