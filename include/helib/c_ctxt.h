#pragma once

#include <helib/c.h>

C_FUNC ctxt_destroy(void *ctxt);

C_FUNC ctxt_get_noise_budget(void *ctxt, long *noise_budget);

C_FUNC ctxt_clone(void **des, void *src);

// Arithmetic

C_FUNC ctxt_add(void **result, void *ctxt1, void *ctxt2);

C_FUNC ctxt_sub(void **result, void *ctxt1, void *ctxt2);

C_FUNC ctxt_negate(void **result, void *ctxt);

C_FUNC ctxt_mult(void **result, void *ctxt1, void *ctxt2);

// Arithmetic with constants

C_FUNC ctxt_add_by_constant(void **result, void *ctxt, void *ptxt_ZZ);

C_FUNC ctxt_sub_by_constant(void **result, void *ctxt, void *ptxt_ZZ);

C_FUNC ctxt_sub_from_constant(void **result, void *ptxt_ZZ, void *ctxt);

C_FUNC ctxt_mult_by_constant(void **result, void *ctxt, void *ptxt_ZZ);

// Arithmetic with ZZX constants

C_FUNC ctxt_add_by_packed_constant(void **result, void *ctxt, void *ptxt_ZZX);

C_FUNC ctxt_sub_by_packed_constant(void **result, void *ctxt, void *ptxt_ZZX);

C_FUNC ctxt_sub_from_packed_constant(void **result, void *ptxt_ZZX, void *ctxt);

C_FUNC ctxt_mult_by_packed_constant(void **result, void *ctxt, void *ptxt_ZZX);

// Arithmetic in place

C_FUNC ctxt_add_inplace(void *ctxt1, void *ctxt2);

C_FUNC ctxt_sub_inplace(void *ctxt1, void *ctxt2);

C_FUNC ctxt_negate_inplace( void *ctxt);

C_FUNC ctxt_mult_inplace(void *ctxt1, void *ctxt2);

// Arithmetic with constants in place

C_FUNC ctxt_add_by_constant_inplace(void *ctxt, void *ptxt_ZZ);

C_FUNC ctxt_sub_by_constant_inplace(void *ctxt, void *ptxt_ZZ);

C_FUNC ctxt_sub_from_constant_inplace(void *ctxt, void *ptxt_ZZ);

C_FUNC ctxt_mult_by_constant_inplace(void *ctxt, void *ptxt_ZZ);


// Arithmetic with ZZX constants in place

C_FUNC ctxt_add_by_packed_constant_inplace(void *ctxt, void *ptxt_ZZX);

C_FUNC ctxt_sub_by_packed_constant_inplace(void *ctxt, void *ptxt_ZZX);

C_FUNC ctxt_sub_from_packed_constant_inplace(void *ctxt, void *ptxt_ZZX);

C_FUNC ctxt_mult_by_packed_constant_inplace(void *ctxt, void *ptxt_ZZX);
