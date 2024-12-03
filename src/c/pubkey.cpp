#include <helib/c/c.h>
#include <helib/helib.h>

C_FUNC pubkey_build(void **pubkey, void *context) {
    helib::Context *context_ = FromVoid<helib::Context>(context);
    IfNullRet(context_, E_POINTER);
    IfNullRet(pubkey, E_POINTER);

    *pubkey = new helib::PubKey(*context_);
    return S_OK;
}

C_FUNC pubkey_destroy(void *pubkey) {
    helib::PubKey *pubkey_ = FromVoid<helib::PubKey>(pubkey);
    IfNullRet(pubkey_, E_POINTER);
    delete pubkey_;
    return S_OK;
}

C_FUNC pubkey_from_seckey(void **pubkey, void *seckey) {
    helib::SecKey *seckey_ = FromVoid<helib::SecKey>(seckey);
    IfNullRet(seckey_, E_POINTER);
    IfNullRet(pubkey, E_POINTER);

    *pubkey = new helib::PubKey(*seckey_);
    return S_OK;
}
