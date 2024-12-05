#include <helib/GaloisKey2k.h>

namespace helib {

size_t GaloisKey2k::get_elt_from_step(int32_t step) {
  const size_t GENERATOR = 3;
  size_t n = m >> 1;
  size_t row_size = n >> 1;
  if (step == 0) {
     return m - 1; // rotate columns
  }

  bool sign = step < 0;
  size_t step_abs = sign ? -step : step;
  if (step_abs >= row_size) {
    throw RuntimeError("DoubleCRT::power_of_two_galois_automorph: Step count too large");
  }
  size_t step_ = sign ? row_size - step_abs : step_abs;

  size_t gen = GENERATOR;
  size_t galois_elt = 1;
  for (size_t i = 0; i < step_; i++) {
    galois_elt = (galois_elt * gen) % m;
  }
  return galois_elt;
}


void GaloisKey2k::generate_step(const SecKey& sKey, int32_t step) {
  size_t galois_elt = get_elt_from_step(step);
  if (keys.find(galois_elt) != keys.end()) {
    return;
  }


  const Context& context = sKey.getContext();
  auto m = context.getZMStar().getM();
  if (m != this->m) {
    throw RuntimeError("GaloisKey2k::generate_step: Mismatched context");
  }
  auto p = context.getP();

  KeySwitch ksMatrix(1, galois_elt, 0, 0);
  RandomBits(ksMatrix.prgSeed, 256); // a random 256-bit seed

  long n = context.getDigits().size();

  // size-n vector
  ksMatrix.b.resize(
      n,
      DoubleCRT(context, context.getCtxtPrimes() | context.getSpecialPrimes()));

  std::vector<DoubleCRT> a;
  a.resize(
      n,
      DoubleCRT(context, context.getCtxtPrimes() | context.getSpecialPrimes()));

  {
    RandomState state;
    SetSeed(ksMatrix.prgSeed);
    for (long i = 0; i < n; i++)
      a[i].randomize();
  } // restore state upon destruction of state

  // Record the plaintext space for this key-switching matrix
  ksMatrix.ptxtSpace = p;


  DoubleCRT fromKey = sKey.sKeys.at(0);    // copy object, not a reference
  fromKey.power_of_two_galois_automorph(galois_elt);
  const DoubleCRT& toKey = sKey.sKeys.at(0); // this can be a reference

  // generate the RLWE instances with pseudorandom ai's
  for (long i = 0; i < n; i++) {
    ksMatrix.noiseBound = RLWE1(ksMatrix.b[i], a[i], toKey, p);
  }
  // Add in the multiples of the fromKey secret key
  fromKey *= context.productOfPrimes(context.getSpecialPrimes());
  for (long i = 0; i < n; i++) {
    ksMatrix.b[i] += fromKey;
    fromKey *= context.productOfPrimes(context.getDigit(i));
  }

  // Push the new matrix onto our list
  keys[galois_elt] = ksMatrix;
}

} // namespace helib
