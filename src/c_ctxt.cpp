#include <helib/c_ctxt.h>
#include <helib/helib.h>

C_FUNC ctxt_destroy(void *ctxt) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    delete ctxt_;
    return S_OK;
}

C_FUNC ctxt_get_noise_budget(void *ctxt, long *noise_budget) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    IfNullRet(noise_budget, E_POINTER);
    *noise_budget = ctxt_->bitCapacity();
    return S_OK;
}

C_FUNC ctxt_clone(void **des, void *src) {
    IfNullRet(des, E_POINTER);
    helib::Ctxt *src_ = FromVoid<helib::Ctxt>(src);
    IfNullRet(src_, E_POINTER);
    *des = new helib::Ctxt(*src_);
    return S_OK;
}

// Arithmetic

C_FUNC ctxt_add(void **result, void *ctxt1, void *ctxt2) {
    helib::Ctxt *ctxt1_ = FromVoid<helib::Ctxt>(ctxt1);
    IfNullRet(ctxt1_, E_POINTER);
    helib::Ctxt *ctxt2_ = FromVoid<helib::Ctxt>(ctxt2);
    IfNullRet(ctxt2_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt1_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    *result_ += *ctxt2_;
    return S_OK;
}

C_FUNC ctxt_sub(void **result, void *ctxt1, void *ctxt2) {
    helib::Ctxt *ctxt1_ = FromVoid<helib::Ctxt>(ctxt1);
    IfNullRet(ctxt1_, E_POINTER);
    helib::Ctxt *ctxt2_ = FromVoid<helib::Ctxt>(ctxt2);
    IfNullRet(ctxt2_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt1_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    *result_ -= *ctxt2_;
    return S_OK;
}

C_FUNC ctxt_negate(void **result, void *ctxt) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    result_->negate();
    return S_OK;
}

C_FUNC ctxt_mult(void **result, void *ctxt1, void *ctxt2) {
    helib::Ctxt *ctxt1_ = FromVoid<helib::Ctxt>(ctxt1);
    IfNullRet(ctxt1_, E_POINTER);
    helib::Ctxt *ctxt2_ = FromVoid<helib::Ctxt>(ctxt2);
    IfNullRet(ctxt2_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt1_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    *result_ *= *ctxt2_;
    return S_OK;
}

// Arithmetic with constants

C_FUNC ctxt_add_by_constant(void **result, void *ctxt, void *ptxt_ZZ) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZ *ptxt_ = FromVoid<NTL::ZZ>(ptxt_ZZ);
    IfNullRet(ptxt_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    result_->addConstant(*ptxt_);
    return S_OK;
}

C_FUNC ctxt_sub_by_constant(void **result, void *ctxt, void *ptxt_ZZ) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZ *ptxt_ = FromVoid<NTL::ZZ>(ptxt_ZZ);
    IfNullRet(ptxt_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    result_->addConstant(*ptxt_, true);
    return S_OK;
}

C_FUNC ctxt_sub_from_constant(void **result, void *ptxt_ZZ, void *ctxt) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZ *ptxt_ = FromVoid<NTL::ZZ>(ptxt_ZZ);
    IfNullRet(ptxt_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    result_->negate();
    result_->addConstant(*ptxt_);
    return S_OK;
}

C_FUNC ctxt_mult_by_constant(void **result, void *ctxt, void *ptxt_ZZ) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZ *ptxt_ = FromVoid<NTL::ZZ>(ptxt_ZZ);
    IfNullRet(ptxt_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    result_->multByConstant(*ptxt_);
    return S_OK;
}

// Arithmetic with ZZX constants

C_FUNC ctxt_add_by_packed_constant(void **result, void *ctxt, void *ptxt_ZZX) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZX *ptxt_ = FromVoid<NTL::ZZX>(ptxt_ZZX);
    IfNullRet(ptxt_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    result_->addConstant(*ptxt_);
    return S_OK;
}

C_FUNC ctxt_sub_by_packed_constant(void **result, void *ctxt, void *ptxt_ZZX) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZX *ptxt_ = FromVoid<NTL::ZZX>(ptxt_ZZX);
    IfNullRet(ptxt_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    result_->addConstant(-*ptxt_);
    return S_OK;
}

C_FUNC ctxt_sub_from_packed_constant(void **result, void *ptxt_ZZX, void *ctxt) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZX *ptxt_ = FromVoid<NTL::ZZX>(ptxt_ZZX);
    IfNullRet(ptxt_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    result_->negate();
    result_->addConstant(*ptxt_);
    return S_OK;
}

C_FUNC ctxt_mult_by_packed_constant(void **result, void *ctxt, void *ptxt_ZZX) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZX *ptxt_ = FromVoid<NTL::ZZX>(ptxt_ZZX);
    IfNullRet(ptxt_, E_POINTER);
    IfNullRet(result, E_POINTER);
    *result = new helib::Ctxt(*ctxt_);
    helib::Ctxt *result_ = FromVoid<helib::Ctxt>(*result);
    IfNullRet(result_, E_POINTER);
    result_->multByConstant(*ptxt_);
    return S_OK;
}

// Arithmetic in place

C_FUNC ctxt_add_inplace(void *ctxt1, void *ctxt2) {
    helib::Ctxt *ctxt1_ = FromVoid<helib::Ctxt>(ctxt1);
    IfNullRet(ctxt1_, E_POINTER);
    helib::Ctxt *ctxt2_ = FromVoid<helib::Ctxt>(ctxt2);
    IfNullRet(ctxt2_, E_POINTER);
    *ctxt1_ += *ctxt2_;
    return S_OK;
}

C_FUNC ctxt_sub_inplace(void *ctxt1, void *ctxt2) {
    helib::Ctxt *ctxt1_ = FromVoid<helib::Ctxt>(ctxt1);
    IfNullRet(ctxt1_, E_POINTER);
    helib::Ctxt *ctxt2_ = FromVoid<helib::Ctxt>(ctxt2);
    IfNullRet(ctxt2_, E_POINTER);
    *ctxt1_ -= *ctxt2_;
    return S_OK;
}

C_FUNC ctxt_negate_inplace( void *ctxt) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    ctxt_->negate();
    return S_OK;
}

C_FUNC ctxt_mult_inplace(void *ctxt1, void *ctxt2) {
    helib::Ctxt *ctxt1_ = FromVoid<helib::Ctxt>(ctxt1);
    IfNullRet(ctxt1_, E_POINTER);
    helib::Ctxt *ctxt2_ = FromVoid<helib::Ctxt>(ctxt2);
    IfNullRet(ctxt2_, E_POINTER);
    *ctxt1_ *= *ctxt2_;
    return S_OK;
}

// Arithmetic with constants in place

C_FUNC ctxt_add_by_constant_inplace(void *ctxt, void *ptxt_ZZ) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZ *ptxt_ = FromVoid<NTL::ZZ>(ptxt_ZZ);
    IfNullRet(ptxt_, E_POINTER);
    ctxt_->addConstant(*ptxt_);
    return S_OK;
}

C_FUNC ctxt_sub_by_constant_inplace(void *ctxt, void *ptxt_ZZ) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZ *ptxt_ = FromVoid<NTL::ZZ>(ptxt_ZZ);
    IfNullRet(ptxt_, E_POINTER);
    ctxt_->addConstant(*ptxt_, true);
    return S_OK;
}

C_FUNC ctxt_sub_from_constant_inplace(void *ctxt, void *ptxt_ZZ) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZ *ptxt_ = FromVoid<NTL::ZZ>(ptxt_ZZ);
    IfNullRet(ptxt_, E_POINTER);
    ctxt_->negate();
    ctxt_->addConstant(*ptxt_);
    return S_OK;
}

C_FUNC ctxt_mult_by_constant_inplace(void *ctxt, void *ptxt_ZZ) {
    helib::Ctxt *ctxt_ = FromVoid<helib::Ctxt>(ctxt);
    IfNullRet(ctxt_, E_POINTER);
    NTL::ZZ *ptxt_ = FromVoid<NTL::ZZ>(ptxt_ZZ);
    IfNullRet(ptxt_, E_POINTER);
    ctxt_->multByConstant(*ptxt_);
    return S_OK;
}
