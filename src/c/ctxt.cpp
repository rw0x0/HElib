#include <helib/c/c.h>
#include <helib/helib.h>

C_FUNC ctxt_destroy(void *ctxt) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    delete ctxt_;
    return S_OK;
}
