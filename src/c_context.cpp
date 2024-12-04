#include <helib/c_context.h>
#include <helib/helib.h>

C_FUNC context_build(void **context, long m, void *p, long bits) {

    NTL::ZZ *p_ = FromVoid<NTL::ZZ>(p);
    IfNullRet(p_, E_POINTER);
    IfNullRet(context, E_POINTER);

    // Hensel lifting (default = 1)
    unsigned long r = 1;
    // Number of columns of Key-Switching matrix (default = 2 or 3)
    unsigned long c = 2;

    *context = helib::ContextBuilder<helib::BGV>()
                                .m(m)
                                .p(*p_)
                                .r(r)
                                .bits(bits)
                                .c(c)
                                .buildPtr();
    return S_OK;
}


C_FUNC context_destroy(void *context) {
    helib::Context *context_ = FromVoid<helib::Context>(context);
    IfNullRet(context_, E_POINTER);
    delete context_;
    return S_OK;
}


C_FUNC context_printout(void *context) {
    helib::Context *context_ = FromVoid<helib::Context>(context);
    IfNullRet(context_, E_POINTER);
    context_->printout();
    return S_OK;
}


C_FUNC context_get_security_level(void *context, double* security_level) {
    helib::Context *context_ = FromVoid<helib::Context>(context);
    IfNullRet(context_, E_POINTER);
    IfNullRet(security_level, E_POINTER);
    *security_level = context_->securityLevel();
    return S_OK;
}
