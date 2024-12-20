/* Copyright (C) 2012-2020 IBM Corp.
 * This program is Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. See accompanying LICENSE file.
 */
#include <cstddef>
#include <tuple>
#include <algorithm>
#include <NTL/BasicThreadPool.h>
#include <helib/matmul.h>
#include <helib/norms.h>
#include <helib/fhe_stats.h>
#include <helib/apiAttributes.h>

#ifndef BIGINT_P
namespace helib {

int fhe_test_force_bsgs = 0;
int fhe_test_force_hoist = 0;

static bool comp_bsgs(bool bsgs)
{
  if (fhe_test_force_bsgs > 0)
    return true;
  if (fhe_test_force_bsgs < 0)
    return false;
  return bsgs;
}

#if 0
static bool comp_hoist(bool hoist)
{
   if (fhe_test_force_hoist > 0) return true;
   if (fhe_test_force_hoist < 0) return false;
   return hoist;
}
#endif

/********************************************************************/
/****************** Auxiliary stuff: should go elsewhere   **********/

/**
 * @class BasicAutomorphPrecon
 * @brief Pre-computation to speed many automorphism on the same ciphertext.
 *
 * The expensive part of homomorphic automorphism is breaking the ciphertext
 * parts into digits. The usual setting is we first rotate the ciphertext
 * parts, then break them into digits. But when we apply many automorphisms
 * it is faster to break the original ciphertext into digits, then rotate
 * the digits (as opposed to first rotate, then break).
 * An BasicAutomorphPrecon object breaks the original ciphertext and keeps
 * the digits, then when you call automorph is only needs to apply the
 * native automorphism and key switching to the digits, which is fast(er).
 **/
class BasicAutomorphPrecon
{
  Ctxt ctxt;
  NTL::xdouble noise;
  std::vector<DoubleCRT> polyDigits;

public:
  BasicAutomorphPrecon(const Ctxt& _ctxt) : ctxt(_ctxt), noise(1.0)
  {
    HELIB_TIMER_START;
    if (ctxt.parts.size() >= 1)
      assertTrue(
          ctxt.parts[0].skHandle.isOne(),
          "Invalid ciphertext (secret key handle for part 0 is not one)");
    if (ctxt.parts.size() <= 1)
      return; // nothing to do

    ctxt.cleanUp();
    const Context& context = ctxt.getContext();
    const PubKey& pubKey = ctxt.getPubKey();
    long keyID = ctxt.getKeyID();

    // The call to cleanUp() should ensure that this assertion passes.
    assertTrue(ctxt.inCanonicalForm(keyID),
               "Ciphertext is not in canonical form");

    ctxt.relin_CKKS_adjust();

    // Compute the number of digits that we need and the estimated
    // added noise from switching this ciphertext.

    NTL::xdouble addedNoise = ctxt.parts[1].breakIntoDigits(polyDigits);
    NTL::xdouble max_ks_noise(0.0);
    for (const KeySwitch& ks : pubKey.keySWlist()) {
      if (max_ks_noise < ks.noiseBound)
        max_ks_noise = ks.noiseBound;
    }
    addedNoise *= max_ks_noise;

    double logProd = context.logOfProduct(context.getSpecialPrimes());
    noise = ctxt.getNoiseBound() * NTL::xexp(logProd);

    double ratio = NTL::conv<double>(addedNoise / noise);

    HELIB_STATS_UPDATE("KS-noise-ratio-hoist", ratio);
    if (ratio > 1) {
      Warning("KS-noise-ratio-hoist=" + std::to_string(ratio));
    }
    // std::stderr << "*** HOIST INIT\n";
    // fprintf(stderr, "   KS-log-noise-ratio-hoist: %f\n",
    // log(addedNoise/noise)/log(2.0));

    noise += addedNoise;
  }

  std::shared_ptr<Ctxt> automorph(long k) const
  {
    HELIB_TIMER_START;

    // A hack: record this automorphism rather than actually performing it
    if (isSetAutomorphVals()) { // defined in NumbTh.h
      recordAutomorphVal(k);
      return std::make_shared<Ctxt>(ctxt);
    }

    if (k == 1 || ctxt.isEmpty())
      return std::make_shared<Ctxt>(ctxt); // nothing to do

    const Context& context = ctxt.getContext();
    const PubKey& pubKey = ctxt.getPubKey();

    // empty ctxt
    std::shared_ptr<Ctxt> result = std::make_shared<Ctxt>(ZeroCtxtLike, ctxt);
    result->noiseBound = noise; // noise estimate
    result->intFactor = ctxt.intFactor;

    result->primeSet = ctxt.primeSet | context.getSpecialPrimes();
    // VJS-NOTE: added this to make addPart work

    if (ctxt.isCKKS()) {
      result->ptxtMag = ctxt.ptxtMag;
      double logProd = context.logOfProduct(context.getSpecialPrimes());
      result->ratFactor = ctxt.ratFactor * NTL::xexp(logProd);
    }

    if (ctxt.parts.size() == 1) { // only constant part, no need to key-switch
      CtxtPart tmpPart = ctxt.parts[0];
      tmpPart.automorph(k);
      tmpPart.addPrimesAndScale(context.getSpecialPrimes());
      result->addPart(tmpPart, /*matchPrimeSet=*/true);
      return result;
    }

    // Ensure that we have a key-switching matrices for this automorphism
    long keyID = ctxt.getKeyID();
    if (!pubKey.isReachable(k, keyID)) {
      throw LogicError("no key-switching matrices for k=" + std::to_string(k) +
                       ", keyID=" + std::to_string(keyID));
    }

    // Get the first key-switching matrix for this automorphism
    const KeySwitch& W = pubKey.getNextKSWmatrix(k, keyID);
    long amt = W.fromKey.getPowerOfX();

    // Start by rotating the constant part, no need to key-switch it
    CtxtPart tmpPart = ctxt.parts[0];
    tmpPart.automorph(amt);
    tmpPart.addPrimesAndScale(context.getSpecialPrimes());
    result->addPart(tmpPart, /*matchPrimeSet=*/true);

    // Then rotate the digits and key-switch them
    std::vector<DoubleCRT> tmpDigits = polyDigits;
    for (auto&& tmp : tmpDigits) // rotate each of the digits
      tmp.automorph(amt);

    result->keySwitchDigits(W, tmpDigits); // key-switch the digits

    long m = context.getM();
    if ((amt - k) % m != 0) { // amt != k (mod m), more automorphisms to do
      k = NTL::MulMod(k, NTL::InvMod(amt, m), m); // k *= amt^{-1} mod m
      result->smartAutomorph(k);                  // call usual smartAutomorph
    }
    return result;
  }
};

class GeneralAutomorphPrecon
{
public:
  virtual ~GeneralAutomorphPrecon() {}

  virtual std::shared_ptr<Ctxt> automorph(long i) const = 0;
};

class GeneralAutomorphPrecon_UNKNOWN : public GeneralAutomorphPrecon
{
private:
  Ctxt ctxt;
  long dim;
  const PAlgebra& zMStar;

public:
  GeneralAutomorphPrecon_UNKNOWN(const Ctxt& _ctxt,
                                 long _dim,
                                 const EncryptedArray& ea) :
      ctxt(_ctxt), dim(_dim), zMStar(ea.getPAlgebra())
  {
    ctxt.cleanUp();
  }

  std::shared_ptr<Ctxt> automorph(long i) const override
  {
    std::shared_ptr<Ctxt> result = std::make_shared<Ctxt>(ctxt);

    // guard against i == 0, as dim may be #gens
    if (i != 0)
      result->smartAutomorph(zMStar.genToPow(dim, i));

    return result;
  }
};

class GeneralAutomorphPrecon_FULL : public GeneralAutomorphPrecon
{
private:
  BasicAutomorphPrecon precon;
  long dim;
  const PAlgebra& zMStar;

public:
  GeneralAutomorphPrecon_FULL(const Ctxt& _ctxt,
                              long _dim,
                              const EncryptedArray& ea) :
      precon(_ctxt), dim(_dim), zMStar(ea.getPAlgebra())
  {}

  std::shared_ptr<Ctxt> automorph(long i) const override
  {
    return precon.automorph(zMStar.genToPow(dim, i));
  }
};

class GeneralAutomorphPrecon_BSGS : public GeneralAutomorphPrecon
{
private:
  long dim;
  const PAlgebra& zMStar;

  long D;
  long g;
  long h;
  std::vector<std::shared_ptr<BasicAutomorphPrecon>> precon;

public:
  GeneralAutomorphPrecon_BSGS(const Ctxt& _ctxt,
                              long _dim,
                              const EncryptedArray& ea) :
      dim(_dim), zMStar(ea.getPAlgebra())
  {
    D = (dim == -1) ? zMStar.getOrdP() : zMStar.OrderOf(dim);
    g = KSGiantStepSize(D);
    h = divc(D, g);

    BasicAutomorphPrecon precon0(_ctxt);
    precon.resize(h);

    // parallel for k in [0..h)
    NTL_EXEC_RANGE(h, first, last)
    for (long k = first; k < last; k++) {
      std::shared_ptr<Ctxt> p = precon0.automorph(zMStar.genToPow(dim, g * k));
      precon[k] = std::make_shared<BasicAutomorphPrecon>(*p);
    }
    NTL_EXEC_RANGE_END
  }

  std::shared_ptr<Ctxt> automorph(long i) const override
  {
    assertInRange(i, 0l, D, "Automorphism index i is not in [0, D)");
    long j = i % g;
    long k = i / g;
    // i == j + g*k
    return precon[k]->automorph(zMStar.genToPow(dim, j));
  }
};

std::shared_ptr<GeneralAutomorphPrecon> buildGeneralAutomorphPrecon(
    const Ctxt& ctxt,
    long dim,
    const EncryptedArray& ea)
{
  // allow dim == -1 (Frobenius)
  // allow dim == #gens (the dummy generator of order 1)
  assertInRange(dim,
                -1l,
                ea.dimension(),
                "Dimension dim is not in [-1, ea.dimension()] (-1 Frobenius)",
                true);

  if (fhe_test_force_hoist >= 0) {
    switch (ctxt.getPubKey().getKSStrategy(dim)) {
    case HELIB_KSS_BSGS:
      return std::make_shared<GeneralAutomorphPrecon_BSGS>(ctxt, dim, ea);

    case HELIB_KSS_FULL:
      return std::make_shared<GeneralAutomorphPrecon_FULL>(ctxt, dim, ea);

    default:
      return std::make_shared<GeneralAutomorphPrecon_UNKNOWN>(ctxt, dim, ea);
    }
  } else {
    return std::make_shared<GeneralAutomorphPrecon_UNKNOWN>(ctxt, dim, ea);
  }
}

/********************************************************************/
/****************** Linear transformation classes *******************/

struct ConstMultiplier
{ // stores a constant in either zzX or DoubleCRT format

  virtual ~ConstMultiplier() {}

  virtual void mul(Ctxt& ctxt) const = 0;

  virtual std::shared_ptr<ConstMultiplier> upgrade(
      const Context& context) const = 0;
  // Upgrade to DCRT. Returns null if no upgrade required
};

struct ConstMultiplier_DoubleCRT : ConstMultiplier
{
  DoubleCRT data;
  double sz;

  ConstMultiplier_DoubleCRT(const DoubleCRT& _data, double _sz) :
      data(_data), sz(_sz)
  {}

  void mul(Ctxt& ctxt) const override { ctxt.multByConstant(data, sz); }

  std::shared_ptr<ConstMultiplier> upgrade(
      UNUSED const Context& context) const override
  {
    return nullptr;
  }
};

struct ConstMultiplier_zzX : ConstMultiplier
{
  zzX data;

  ConstMultiplier_zzX(const zzX& _data) : data(_data) {}

  void mul(Ctxt& ctxt) const override { ctxt.multByConstant(data); }

  std::shared_ptr<ConstMultiplier> upgrade(
      const Context& context) const override
  {
    double sz = embeddingLargestCoeff(data, context.getZMStar());

    return std::make_shared<ConstMultiplier_DoubleCRT>(
        DoubleCRT(data, context, context.fullPrimes()),
        sz);
  }
};

template <typename RX>
std::shared_ptr<ConstMultiplier> build_ConstMultiplier(const RX& poly)
{
  if (IsZero(poly))
    return nullptr;
  else
    return std::make_shared<ConstMultiplier_zzX>(balanced_zzX(poly));
}

template <typename RX, typename type>
std::shared_ptr<ConstMultiplier> build_ConstMultiplier(
    const RX& poly,
    long dim,
    long amt,
    const EncryptedArrayDerived<type>& ea)
{
  if (IsZero(poly))
    return nullptr;
  else {
    RX poly1;
    plaintextAutomorph(poly1, poly, dim, amt, ea);
    return std::make_shared<ConstMultiplier_zzX>(balanced_zzX(poly1));
  }
}

void MulAdd(Ctxt& x, const std::shared_ptr<ConstMultiplier>& a, const Ctxt& b)
// x += a*b
{
  if (a) {
    Ctxt tmp(b);
    a->mul(tmp);
    x += tmp;
  }
}

void DestMulAdd(Ctxt& x, const std::shared_ptr<ConstMultiplier>& a, Ctxt& b)
// x += a*b, b may be modified
{
  if (a) {
    a->mul(b);
    x += b;
  }
}

void ConstMultiplierCache::upgrade(const Context& context)
{
  HELIB_TIMER_START;

  long n = multiplier.size();
  NTL_EXEC_RANGE(n, first, last)
  for (long i : range(first, last)) {
    if (multiplier[i])
      if (auto newptr = multiplier[i]->upgrade(context))
        multiplier[i] = std::shared_ptr<ConstMultiplier>(newptr);
  }
  NTL_EXEC_RANGE_END
}

static inline long dimSz(const EncryptedArray& ea, long dim)
{
  return (dim == ea.dimension()) ? 1 : ea.sizeOfDimension(dim);
}

static inline long dimSz(const EncryptedArrayBase& ea, long dim)
{
  return (dim == ea.dimension()) ? 1 : ea.sizeOfDimension(dim);
}

static inline long dimNative(const EncryptedArray& ea, long dim)
{
  return (dim == ea.dimension()) ? true : ea.nativeDimension(dim);
}

static inline long dimNative(const EncryptedArrayBase& ea, long dim)
{
  return (dim == ea.dimension()) ? true : ea.nativeDimension(dim);
}

template <typename type>
struct MatMul1D_derived_impl
{
  PA_INJECT(type)

  static void processDiagonal1(RX& poly,
                               long i,
                               const EncryptedArrayDerived<type>& ea,
                               const MatMul1D_derived<type>& mat)
  {
    long dim = mat.getDim();
    long D = dimSz(ea, dim);

    std::vector<RX> tmpDiag(D);
    bool zDiag = true; // is this a zero diagonal?
    long nzLast = -1;  // index of last non-zero entry
    RX entry;

    // Process the entries in this diagonal one at a time
    for (long j = 0; j < D; j++) { // process entry j
      bool zEntry = mat.get(entry, mcMod(j - i, D), j, 0);
      // entry [j-i mod D, j]

      assertTrue(zEntry || deg(entry) < ea.getDegree(),
                 "Entry is non zero and degree of entry greater or "
                 "equal than ea.getDegree()");
      // get(...) returns true if the entry is empty, false otherwise

      if (!zEntry && IsZero(entry))
        zEntry = true; // zero is an empty entry too

      if (!zEntry) {   // not a zero entry
        zDiag = false; // mark diagonal as non-empty

        // clear entries between last nonzero entry and this one
        for (long jj = nzLast + 1; jj < j; jj++)
          clear(tmpDiag[jj]);
        tmpDiag[j] = entry;
        nzLast = j;
      }
    }
    if (zDiag) {
      clear(poly);
    } else {

      // clear trailing zero entries
      for (long jj = nzLast + 1; jj < D; jj++)
        clear(tmpDiag[jj]);

      std::vector<RX> diag(ea.size());
      if (D == 1)
        diag.assign(ea.size(), tmpDiag[0]); // dimension of size one
      else {
        for (long j = 0; j < ea.size(); j++)
          diag[j] = tmpDiag[ea.coordinate(dim, j)];
        // rearrange the indexes based on the current dimension
      }

      ea.encode(poly, diag);
    }
  }

  static void processDiagonal2(RX& poly,
                               long idx,
                               const EncryptedArrayDerived<type>& ea,
                               const MatMul1D_derived<type>& mat)
  {
    long dim = mat.getDim();
    long D = dimSz(ea, dim);

    bool zDiag = true; // is this a zero diagonal?
    long nzLast = -1;  // index of last non-zero entry
    RX entry;

    long n = ea.size();

    // Process the entries in this diagonal one at a time
    long blockIdx, innerIdx;
    std::vector<RX> diag(n);
    for (long j = 0; j < n; j++) {
      if (D == 1) {
        blockIdx = j;
        innerIdx = 0;
      } else {
        std::tie(blockIdx, innerIdx) // std::pair<long,long> idxes
            = ea.getPAlgebra().breakIndexByDim(j, dim);
        //	blockIdx = idxes.first;  // which transformation
        //	innerIdx = idxes.second; // index along dimension dim
      }
      // process entry j
      bool zEntry =
          mat.get(entry, mcMod(innerIdx - idx, D), innerIdx, blockIdx);
      // entry [i,j-i mod D] in the block corresponding to blockIdx
      // get(...) returns true if the entry is empty, false otherwise

      // If non-zero, make sure the degree is not too large
      assertTrue(zEntry || deg(entry) < ea.getDegree(),
                 "Entry is non zero and degree of entry greater or "
                 "equal than ea.getDegree()");

      if (!zEntry && IsZero(entry))
        zEntry = true; // zero is an empty entry too

      if (!zEntry) {   // not a zero entry
        zDiag = false; // mark diagonal as non-empty

        // clear entries between last nonzero entry and this one
        for (long jj = nzLast + 1; jj < j; jj++)
          clear(diag[jj]);
        nzLast = j;
        diag[j] = entry;
      }
    }
    if (zDiag) {
      clear(poly);
    } else {

      // clear trailing zero entries
      for (long jj = nzLast + 1; jj < ea.size(); jj++)
        clear(diag[jj]);

      ea.encode(poly, diag);
    }
  }

  // Get the i'th diagonal, encoded as a single constant.
  static void processDiagonal(RX& poly,
                              long i,
                              const EncryptedArrayDerived<type>& ea,
                              const MatMul1D_derived<type>& mat)
  {
    if (mat.multipleTransforms())
      processDiagonal2(poly, i, ea, mat);
    else
      processDiagonal1(poly, i, ea, mat);
  }
};

template <typename type>
void MatMul1D_derived<type>::processDiagonal(
    RX& poly,
    long i,
    const EncryptedArrayDerived<type>& ea) const
{
  MatMul1D_derived_impl<type>::processDiagonal(poly, i, ea, *this);
}

// explicit instantiations
template void MatMul1D_derived<PA_GF2>::processDiagonal(
    RX& poly,
    long i,
    const EncryptedArrayDerived<PA_GF2>& ea) const;

template void MatMul1D_derived<PA_zz_p>::processDiagonal(
    RX& poly,
    long i,
    const EncryptedArrayDerived<PA_zz_p>& ea) const;

#define ALT_MATMUL (1)

template <typename type>
struct MatMul1DExec_construct
{
  PA_INJECT(type)

  static void apply(const EncryptedArrayDerived<type>& ea,
                    const MatMul1D& mat_basetype,
                    std::vector<std::shared_ptr<ConstMultiplier>>& vec,
                    std::vector<std::shared_ptr<ConstMultiplier>>& vec1,
                    long g)
  {
    const MatMul1D_partial<type>& mat =
        dynamic_cast<const MatMul1D_partial<type>&>(mat_basetype);

    long dim = mat.getDim();
    long D = dimSz(ea, dim);
    bool native = dimNative(ea, dim);

    RBak bak;
    bak.save();
    ea.getTab().restoreContext();

    if (native) {

      vec.resize(D);

      for (long i : range(D)) {
        // i == j + g * k (where j = (g != 0) ? i % g : i)
        long k;

        if (g) {
          k = i / g;
        } else {
          k = 1;
        }

        RX poly;
        mat.processDiagonal(poly, i, ea);
        vec[i] = build_ConstMultiplier(poly, dim, -g * k, ea);
      }
    } else {
      vec.resize(D);
      vec1.resize(D);

      for (long i : range(D)) {
        // i == j + g * k (where j = (g != 0) ? i % g : i)
        long k;

        if (g) {
          k = i / g;
        } else {
          k = 1;
        }

        RX poly;
        mat.processDiagonal(poly, i, ea);

        if (IsZero(poly)) {
          vec[i] = nullptr;
          vec1[i] = nullptr;
          continue;
        }

        const RX& mask = ea.getTab().getMaskTable()[dim][i];
        const RXModulus& PhimXMod = ea.getTab().getPhimXMod();

        RX poly1, poly2;
        MulMod(poly1, poly, mask, PhimXMod);
        sub(poly2, poly, poly1);

        // poly1 = poly w/ first i slots zeroed out
        // poly2 = poly w/ last D-i slots zeroed out

        vec[i] = build_ConstMultiplier(poly1, dim, -g * k, ea);

#if (ALT_MATMUL)
        long DD = D;
        if (g)
          DD = 0;
        vec1[i] = build_ConstMultiplier(poly2, dim, DD - g * k, ea);
#else
        vec1[i] = build_ConstMultiplier(poly2, dim, D - g * k, ea);
#endif
      }
    }
  }
};

HELIB_NO_CKKS_IMPL(MatMul1DExec_construct)

// HERE
void MatMul1D_CKKS::processDiagonal(std::vector<std::complex<double>>& diag,
                                    long i,
                                    const EncryptedArrayCx& ea) const
{
  long D = ea.size();

  diag.resize(D);

  // Process the entries in this diagonal one at a time
  for (long j : range(D)) { // process entry j
    diag[j] = this->get(mcMod(j - i, D), j);
  }
}

#if 0
// This is no longer needed. We can eventually get rid of it
void plaintextAutomorph_CKKS(zzX& b,
                             const zzX& a,
                             long j,
                             const EncryptedArrayCx& ea)
{
  const PAlgebra& zMStar = ea.getPAlgebra();
  long k = zMStar.genToPow(0, j);
  long m = zMStar.getM();
  long d = a.length() - 1;

  // compute b(X) = a(X^k) mod (X^{m/2}+1)
  if (k == 1 || d <= 0) {
    b = a;
    return;
  }

  b.SetLength(m / 2);
  NTL::mulmod_precon_t precon = NTL::PrepMulModPrecon(k, m);

  for (long j = 0; j <= d; j++) {
    long pos = NTL::MulModPrecon(j, k, m, precon);
    long c = a[j];
    if (pos >= m / 2) {
      c = -c;
      pos -= m / 2;
    }
    b[pos] = c;
  }

  normalize(b);
}
#endif

struct ConstMultiplier_DoubleCRT_CKKS : ConstMultiplier
{
  FatEncodedPtxt feptxt;

  ConstMultiplier_DoubleCRT_CKKS(const EncodedPtxt& eptxt, const IndexSet& s)
  {
    feptxt.expand(eptxt, s);
  }

  void mul(Ctxt& ctxt) const override { ctxt *= feptxt; }

  std::shared_ptr<ConstMultiplier> upgrade(
      UNUSED const Context& context) const override
  {
    return nullptr;
  }
};

struct ConstMultiplier_zzX_CKKS : ConstMultiplier
{
  EncodedPtxt eptxt;

  ConstMultiplier_zzX_CKKS(const std::vector<std::complex<double>>& diag,
                           const EncryptedArrayCx& ea)
  {
    ea.encode(eptxt, diag);
  }

  void mul(Ctxt& ctxt) const override { ctxt *= eptxt; }

  std::shared_ptr<ConstMultiplier> upgrade(
      const Context& context) const override
  {
    return std::make_shared<ConstMultiplier_DoubleCRT_CKKS>(
        eptxt,
        context.fullPrimes());
  }
};

static std::shared_ptr<ConstMultiplier> build_ConstMultiplier_CKKS(
    const std::vector<std::complex<double>>& diag,
    long amt,
    const EncryptedArrayCx& ea)
{
  double size = Norm(diag);
  if (size == 0.0)
    return nullptr;
  else {
    long n = ea.size();

    // diag1 = diag rotated by amt...could be optimized for amt==0
    std::vector<std::complex<double>> diag1(n);
    for (long i : range(n))
      diag1[((i + amt) % n + n) % n] = diag[i];

    return std::make_shared<ConstMultiplier_zzX_CKKS>(diag1, ea);
  }
}

static void MatMul1DExec_construct_CKKS(
    const EncryptedArrayCx& ea,
    const MatMul1D& mat_basetype,
    std::vector<std::shared_ptr<ConstMultiplier>>& vec,
    long g)
{
  const MatMul1D_CKKS& mat = dynamic_cast<const MatMul1D_CKKS&>(mat_basetype);

  long dim = mat.getDim();
  long D = dimSz(ea, dim);
  bool native = dimNative(ea, dim);

  if (dim != 0 || D != ea.size() || !native)
    throw LogicError("MatMul1DExec_construct_CKKS: bad params");

  vec.resize(D);

  for (long i : range(D)) {
    // i == j + g * k (where j = (g != 0) ? i % g : i)
    long k;

    if (g) {
      k = i / g;
    } else {
      k = 1;
    }

    std::vector<std::complex<double>> diag;
    mat.processDiagonal(diag, i, ea);
    vec[i] = build_ConstMultiplier_CKKS(diag, -g * k, ea);
  }
}

#define HELIB_BSGS_MUL_THRESH HELIB_KEYSWITCH_THRESH
// uses a BSGS multiplication strategy if sizeof(dim) > HELIB_BSGS_MUL_THRESH;
// otherwise uses the old strategy (but potentially with hoisting)

// For performance purposes, should not exceed HELIB_KEYSWITCH_THRESH
// For testing purposes:
//    set to 1 to always use BSGS
//    set to infty to never use BSGS

MatMul1DExec::MatMul1DExec(const MatMul1D& mat, bool _minimal) :
    ea(mat.getEA()), minimal(_minimal)
{
  HELIB_NTIMER_START(MatMul1DExec);

  dim = mat.getDim();
  assertInRange(dim,
                0l,
                ea.dimension(),
                "Matrix dimension not in [0, ea.dimension()]",
                true);

  // VJS-FIXME: we actually do use dim==ea.dimension() in bootstrapping.
  // We should document this feature, at least for internal purposes.
  // The basic idea is that this dimension is treated as a dimension
  // of size 1.
  // if (dim == ea.dimension()) std::cerr << "*** MatMul1DExec: big dim\n";

  D = dimSz(ea, dim);
  native = dimNative(ea, dim);

  bool bsgs = comp_bsgs(D > HELIB_BSGS_MUL_THRESH ||
                        (minimal && D > HELIB_KEYSWITCH_MIN_THRESH));

  if (!bsgs)
    g = 0; // do not use BSGS
  else
    g = KSGiantStepSize(D); // use BSGS

  if (ea.getTag() == PA_cx_tag) {
    MatMul1DExec_construct_CKKS(ea.getCx(), mat, cache.multiplier, g);
  } else {
    ea.dispatch<MatMul1DExec_construct>(mat,
                                        cache.multiplier,
                                        cache1.multiplier,
                                        g);
  }
}

/***************************************************************************

BS/GS logic:

  \sum_{i=0}^{D-1} const_i rot^i(v)
    = \sum_k \sum_j const_{j+g*k} rot^{j+g*k}(v)
    = \sum_k rot^{g*k}[ \sum_j rot^{-g*k}(const_{j+g*k}) rot^j(v) ]

So we first compute baby_steps[j] = rot^j(v) for j in [0..g).
Then for each k in [0..ceil(D/g)), we compute
   giant_steps[k] = \rot^{g*k}[ rot^{-g*k}(const_{j+g*k}) baby_steps[j] ]
Then we add up all the giant_steps.

In bad dimensions:

We need to compute
\[
  \sum_{j,k} c_{j+gk} r^{j+gk}(x)
\]
where $r^i$ denotes rotation by $i$.
In bad dimensions, we have
\[
 r^i(x) = d_i \rho^i(x) + e_i \rho^{i-D}(x)
\]
for constants $d_i$ and $e_i$.
Here, d_i is maskTable[i][amt] and e_i = 1-d_i

So putting it all together
\[
  \sum_{j,k} c_{j+gk} r^{j+gk}(x)
= \sum_{j,k} d'_{j+gk} \rho^{j+gk}(x) + e'_{j+gk} \rho^{j+gk-D}(x)
     \text{where $d'_i=c_i d_i$ and $e'_i = c_i e_i$}


=               \sum_k \rho^{gk}[ \sum_j d''_{j+gk} \rho^j(x) ]
   + \rho^{-D}[ \sum_k \rho^{gk}[ \sum_j e''_{j+gk} \rho^j(x) ] ]
      \text{where $d''_{j+gk} = \rho^{-gk}(d'_{j+gk})$ and
                  $e''_{j+gk} = \rho^{D-gk}(d'_{j+gk})$}

\]

***************************************************************************/

void GenBabySteps(std::vector<std::shared_ptr<Ctxt>>& v,
                  const Ctxt& ctxt,
                  long dim,
                  bool clean)
{
  long n = v.size();
  assertTrue<InvalidArgument>(n > 0, "Empty vector v");

  if (n == 1) {
    v[0] = std::make_shared<Ctxt>(ctxt);
    if (clean)
      v[0]->cleanUp();
    return;
  }

  const PAlgebra& zMStar = ctxt.getContext().getZMStar();

  // std::cerr << "*** STRATEGY FOR dim " << dim << " = " <<
  // ctxt.getPubKey().getKSStrategy(dim) << "\n";

  if (fhe_test_force_hoist >= 0 &&
      ctxt.getPubKey().getKSStrategy(dim) != HELIB_KSS_UNKNOWN) {
    BasicAutomorphPrecon precon(ctxt);

    NTL_EXEC_RANGE(n, first, last)
    for (long j : range(first, last)) {
      v[j] = precon.automorph(zMStar.genToPow(dim, j));
      if (clean)
        v[j]->cleanUp();
    }
    NTL_EXEC_RANGE_END
  } else {
    Ctxt ctxt0(ctxt);
    ctxt0.cleanUp();

    NTL_EXEC_RANGE(n, first, last)
    for (long j : range(first, last)) {
      v[j] = std::make_shared<Ctxt>(ctxt0);
      v[j]->smartAutomorph(zMStar.genToPow(dim, j));
      if (clean)
        v[j]->cleanUp();
    }
    NTL_EXEC_RANGE_END
  }
}

void MatMul1DExec::mul(Ctxt& ctxt) const
{
  HELIB_NTIMER_START(mul_MatMul1DExec);

  assertEq(&ea.getContext(),
           &ctxt.getContext(),
           "Cannot multiply ciphertexts with context different to "
           "encrypted array one");
  const PAlgebra& zMStar = ea.getPAlgebra();

  ctxt.cleanUp();

  bool iterative = false;
  if (ctxt.getPubKey().getKSStrategy(dim) == HELIB_KSS_MIN)
    iterative = true;

  if (g != 0) {
    // baby-step / giant-step

    if (native) {

      if (iterative) {

        std::vector<Ctxt> baby_steps(g, Ctxt(ZeroCtxtLike, ctxt));
        baby_steps[0] = ctxt;
        for (long j : range(1, g)) {
          baby_steps[j] = baby_steps[j - 1];
          baby_steps[j].smartAutomorph(zMStar.genToPow(dim, 1));
          baby_steps[j].cleanUp();
        }

        long h = divc(D, g);
        Ctxt sum(ZeroCtxtLike, ctxt);
        for (long k = h - 1; k >= 0; k--) {
          if (k < h - 1) {
            sum.smartAutomorph(zMStar.genToPow(dim, g));
            sum.cleanUp();
          }

          for (long j : range(g)) {
            long i = j + g * k;
            if (i >= D)
              break;
            MulAdd(sum, cache.multiplier[i], baby_steps[j]);
          }
        }

        ctxt = sum;

      } else {

        long h = divc(D, g);
        std::vector<std::shared_ptr<Ctxt>> baby_steps(g);
        GenBabySteps(baby_steps, ctxt, dim, true);

        NTL::PartitionInfo pinfo(h);
        long cnt = pinfo.NumIntervals();

        std::vector<Ctxt> acc(cnt, Ctxt(ZeroCtxtLike, ctxt));

        // parallel for loop: k in [0..h)
        NTL_EXEC_INDEX(cnt, index)
        long first, last;
        pinfo.interval(first, last, index);

        for (long k : range(first, last)) {
          Ctxt acc_inner(ZeroCtxtLike, ctxt);

          for (long j : range(g)) {
            long i = j + g * k;
            if (i >= D)
              break;
            MulAdd(acc_inner, cache.multiplier[i], *baby_steps[j]);
          }

          if (k > 0)
            acc_inner.smartAutomorph(zMStar.genToPow(dim, g * k));
          acc[index] += acc_inner;
        }
        NTL_EXEC_INDEX_END

        ctxt = acc[0];
        for (long i : range(1, cnt))
          ctxt += acc[i];
      }
    } else {
#if (ALT_MATMUL)
      if (iterative) {

        std::vector<Ctxt> baby_steps(g, Ctxt(ZeroCtxtLike, ctxt));
        baby_steps[0] = ctxt;
        for (long j : range(1, g)) {
          baby_steps[j] = baby_steps[j - 1];
          baby_steps[j].smartAutomorph(zMStar.genToPow(dim, 1));
          baby_steps[j].cleanUp();
        }

        std::vector<Ctxt> baby_steps1(g, Ctxt(ZeroCtxtLike, ctxt));
        baby_steps1[0] = ctxt;
        baby_steps1[0].smartAutomorph(zMStar.genToPow(dim, -D));

        for (long j : range(1, g)) {
          baby_steps1[j] = baby_steps1[j - 1];
          baby_steps1[j].smartAutomorph(zMStar.genToPow(dim, 1));
          baby_steps1[j].cleanUp();
        }

        long h = divc(D, g);
        Ctxt sum(ZeroCtxtLike, ctxt);
        for (long k = h - 1; k >= 0; k--) {
          if (k < h - 1) {
            sum.smartAutomorph(zMStar.genToPow(dim, g));
            sum.cleanUp();
          }

          for (long j : range(g)) {
            long i = j + g * k;
            if (i >= D)
              break;
            MulAdd(sum, cache.multiplier[i], baby_steps[j]);
            MulAdd(sum, cache1.multiplier[i], baby_steps1[j]);
          }
        }
        ctxt = sum;
      } else {
        long h = divc(D, g);
        std::vector<std::shared_ptr<Ctxt>> baby_steps(g);
        std::vector<std::shared_ptr<Ctxt>> baby_steps1(g);

        GenBabySteps(baby_steps, ctxt, dim, false);

        Ctxt ctxt1(ctxt);
        ctxt1.smartAutomorph(zMStar.genToPow(dim, -D));
        GenBabySteps(baby_steps1, ctxt1, dim, false);

        NTL::PartitionInfo pinfo(h);
        long cnt = pinfo.NumIntervals();

        std::vector<Ctxt> acc(cnt, Ctxt(ZeroCtxtLike, ctxt));

        // parallel for loop: k in [0..h)
        NTL_EXEC_INDEX(cnt, index)

        long first, last;
        pinfo.interval(first, last, index);

        for (long k : range(first, last)) {
          Ctxt acc_inner(ZeroCtxtLike, ctxt);

          for (long j : range(g)) {
            long i = j + g * k;
            if (i >= D)
              break;
            MulAdd(acc_inner, cache.multiplier[i], *baby_steps[j]);
            MulAdd(acc_inner, cache1.multiplier[i], *baby_steps1[j]);
          }

          if (k > 0) {
            acc_inner.smartAutomorph(zMStar.genToPow(dim, g * k));
          }

          acc[index] += acc_inner;
        }

        NTL_EXEC_INDEX_END

        for (long i : range(1, cnt))
          acc[0] += acc[i];
        ctxt = acc[0];
      }
#else
      if (iterative) {

        std::vector<Ctxt> baby_steps(g, Ctxt(ZeroCtxtLike, ctxt));
        baby_steps[0] = ctxt;
        for (long j : range(1, g)) {
          baby_steps[j] = baby_steps[j - 1];
          baby_steps[j].smartAutomorph(zMStar.genToPow(dim, 1));
          baby_steps[j].cleanUp();
        }

        long h = divc(D, g);
        Ctxt sum(ZeroCtxtLike, ctxt);
        Ctxt sum1(ZeroCtxtLike, ctxt);
        for (long k = h - 1; k >= 0; k--) {
          if (k < h - 1) {
            sum.smartAutomorph(zMStar.genToPow(dim, g));
            sum.cleanUp();
            sum1.smartAutomorph(zMStar.genToPow(dim, g));
            sum1.cleanUp();
          }

          for (long j : range(g)) {
            long i = j + g * k;
            if (i >= D)
              break;
            MulAdd(sum, cache.multiplier[i], baby_steps[j]);
            MulAdd(sum1, cache1.multiplier[i], baby_steps[j]);
          }
        }
        sum1.smartAutomorph(zMStar.genToPow(dim, -D));
        sum += sum1;
        ctxt = sum;
      } else {
        long h = divc(D, g);
        std::vector<std::shared_ptr<Ctxt>> baby_steps(g);
        GenBabySteps(baby_steps, ctxt, dim, true);

        NTL::PartitionInfo pinfo(h);
        long cnt = pinfo.NumIntervals();

        std::vector<Ctxt> acc(cnt, Ctxt(ZeroCtxtLike, ctxt));
        std::vector<Ctxt> acc1(cnt, Ctxt(ZeroCtxtLike, ctxt));

        // parallel for loop: k in [0..h)
        NTL_EXEC_INDEX(cnt, index)

        long first, last;
        pinfo.interval(first, last, index);

        for (long k : range(first, last)) {
          Ctxt acc_inner(ZeroCtxtLike, ctxt);
          Ctxt acc_inner1(ZeroCtxtLike, ctxt);

          for (long j : range(g)) {
            long i = j + g * k;
            if (i >= D)
              break;
            MulAdd(acc_inner, cache.multiplier[i], *baby_steps[j]);
            MulAdd(acc_inner1, cache1.multiplier[i], *baby_steps[j]);
          }

          if (k > 0) {
            acc_inner.smartAutomorph(zMStar.genToPow(dim, g * k));
            acc_inner1.smartAutomorph(zMStar.genToPow(dim, g * k));
          }

          acc[index] += acc_inner;
          acc1[index] += acc_inner1;
        }
        NTL_EXEC_INDEX_END

        for (long i : range(1, cnt))
          acc[0] += acc[i];
        for (long i : range(1, cnt))
          acc1[0] += acc1[i];

        acc1[0].smartAutomorph(zMStar.genToPow(dim, -D));
        acc[0] += acc1[0];
        ctxt = acc[0];
      }
#endif
    }
  } else if (!iterative) {
    if (native) {
      std::shared_ptr<GeneralAutomorphPrecon> precon =
          buildGeneralAutomorphPrecon(ctxt, dim, ea);

      NTL::PartitionInfo pinfo(D);
      long cnt = pinfo.NumIntervals();

      std::vector<Ctxt> acc(cnt, Ctxt(ZeroCtxtLike, ctxt));

      // parallel for loop: i in [0..D)
      NTL_EXEC_INDEX(cnt, index)
      long first, last;
      pinfo.interval(first, last, index);

      for (long i : range(first, last)) {
        if (cache.multiplier[i]) {
          std::shared_ptr<Ctxt> tmp = precon->automorph(i);
          DestMulAdd(acc[index], cache.multiplier[i], *tmp);
        }
      }
      NTL_EXEC_INDEX_END

      ctxt = acc[0];
      for (long i : range(1, cnt)) {
        ctxt += acc[i];
      }
    } else {
      std::shared_ptr<GeneralAutomorphPrecon> precon =
          buildGeneralAutomorphPrecon(ctxt, dim, ea);

      NTL::PartitionInfo pinfo(D);
      long cnt = pinfo.NumIntervals();

      std::vector<Ctxt> acc(cnt, Ctxt(ZeroCtxtLike, ctxt));
      std::vector<Ctxt> acc1(cnt, Ctxt(ZeroCtxtLike, ctxt));

      // parallel for loop: i in [0..D)
      NTL_EXEC_INDEX(cnt, index)
      long first, last;
      pinfo.interval(first, last, index);

      for (long i : range(first, last)) {
        if (cache.multiplier[i] || cache1.multiplier[i]) {
          std::shared_ptr<Ctxt> tmp = precon->automorph(i);
          MulAdd(acc[index], cache.multiplier[i], *tmp);
          DestMulAdd(acc1[index], cache1.multiplier[i], *tmp);
        }
      }
      NTL_EXEC_INDEX_END

      for (long i : range(1, cnt))
        acc[0] += acc[i];
      for (long i : range(1, cnt))
        acc1[0] += acc1[i];

      acc1[0].smartAutomorph(zMStar.genToPow(dim, -D));
      acc[0] += acc1[0];
      ctxt = acc[0];
    }
  } else /* iterative */ {
    if (native) {
      Ctxt acc(ZeroCtxtLike, ctxt);
      Ctxt sh_ctxt(ctxt);

      for (long i : range(D)) {
        if (i > 0) {
          sh_ctxt.smartAutomorph(zMStar.genToPow(dim, 1));
          sh_ctxt.cleanUp();
        }
        MulAdd(acc, cache.multiplier[i], sh_ctxt);
      }

      ctxt = acc;
    } else {
      Ctxt acc(ZeroCtxtLike, ctxt);
      Ctxt acc1(ZeroCtxtLike, ctxt);
      Ctxt sh_ctxt(ctxt);

      for (long i : range(D)) {
        if (i > 0) {
          sh_ctxt.smartAutomorph(zMStar.genToPow(dim, 1));
          sh_ctxt.cleanUp();
        }
        MulAdd(acc, cache.multiplier[i], sh_ctxt);
        MulAdd(acc1, cache1.multiplier[i], sh_ctxt);
      }

      acc1.smartAutomorph(zMStar.genToPow(dim, -D));
      acc += acc1;
      ctxt = acc;
    }
  }
}

// ========================== BlockMatMul1D stuff =====================

template <typename type>
struct BlockMatMul1D_derived_impl
{
  PA_INJECT(type)

  // return true if zero
  static bool processDiagonal1(std::vector<RX>& poly,
                               long i,
                               const EncryptedArrayDerived<type>& ea,
                               const BlockMatMul1D_derived<type>& mat)
  {
    long dim = mat.getDim();
    long D = dimSz(ea, dim);
    long nslots = ea.size();
    long d = ea.getDegree();

    bool zDiag = true; // is this a zero diagonal?
    long nzLast = -1;  // index of last non-zero entry

    mat_R entry(NTL::INIT_SIZE, d, d);
    std::vector<RX> entry1(d);
    std::vector<std::vector<RX>> tmpDiag(D);

    std::vector<std::vector<RX>> diag(nslots);

    // Process the entries in this diagonal one at a time
    for (long j : range(D)) { // process entry j
      bool zEntry =
          mat.get(entry, mcMod(j - i, D), j, 0); // entry [j-i mod D, j]
      // get(...) returns true if the entry is empty, false otherwise

      if (!zEntry && IsZero(entry))
        zEntry = true; // zero is an empty entry too
      assertTrue(zEntry || (entry.NumRows() == d && entry.NumCols() == d),
                 "Non zero entry and number of entry rows and columns "
                 "are not equal to d");

      if (!zEntry) {   // not a zero entry
        zDiag = false; // mark diagonal as non-empty

        for (long jj : range(nzLast + 1, j)) { // clear from last nonzero entry
          tmpDiag[jj].assign(d, RX());
        }
        nzLast = j; // current entry is the last nonzero one

        // recode entry as a vector of polynomials
        for (long k : range(d))
          conv(entry1[k], entry[k]);

        // compute the linearized polynomial coefficients
        ea.buildLinPolyCoeffs(tmpDiag[j], entry1);
      }
    }
    if (zDiag)
      return true; // zero diagonal, nothing to do

    // clear trailing zero entries
    for (long jj : range(nzLast + 1, D)) {
      tmpDiag[jj].assign(d, RX());
    }

    if (D == 1)
      diag.assign(nslots, tmpDiag[0]); // dimension of size one
    else {
      for (long j : range(nslots))
        diag[j] = tmpDiag[ea.coordinate(dim, j)];
      // rearrange the indexes based on the current dimension
    }

    // transpose and encode diag to form polys

    std::vector<RX> slots(nslots);
    poly.resize(d);
    for (long i : range(d)) {
      for (long j : range(nslots))
        slots[j] = diag[j][i];
      ea.encode(poly[i], slots);
    }

    return false; // a nonzero diagonal
  }

  // return true if zero
  static bool processDiagonal2(std::vector<RX>& poly,
                               long idx,
                               const EncryptedArrayDerived<type>& ea,
                               const BlockMatMul1D_derived<type>& mat)
  {
    long dim = mat.getDim();
    long D = dimSz(ea, dim);
    long nslots = ea.size();
    long d = ea.getDegree();

    bool zDiag = true; // is this a zero diagonal?
    long nzLast = -1;  // index of last non-zero entry

    mat_R entry(NTL::INIT_SIZE, d, d);
    std::vector<RX> entry1(d);

    std::vector<std::vector<RX>> diag(nslots);

    // Get the slots in this diagonal one at a time
    long blockIdx, rowIdx, colIdx;
    for (long j : range(nslots)) { // process entry j
      if (dim == ea.dimension()) { // "special" last dimension of size 1
        rowIdx = colIdx = 0;
        blockIdx = j;
      } else {
        std::tie(blockIdx, colIdx) = ea.getPAlgebra().breakIndexByDim(j, dim);
        rowIdx = mcMod(colIdx - idx, D);
      }
      bool zEntry = mat.get(entry, rowIdx, colIdx, blockIdx);
      // entry [i,j-i mod D] in the block corresponding to blockIdx
      // get(...) returns true if the entry is empty, false otherwise

      if (!zEntry && IsZero(entry))
        zEntry = true; // zero is an empty entry too
      assertTrue(zEntry || (entry.NumRows() == d && entry.NumCols() == d),
                 "Non zero entry and number of entry rows and columns "
                 "are not equal to d");

      if (!zEntry) {   // non-empty entry
        zDiag = false; // mark diagonal as non-empty

        for (long jj : range(nzLast + 1, j)) // clear from last nonzero entry
          diag[jj].assign(d, RX());

        nzLast = j; // current entry is the last nonzero one

        // recode entry as a vector of polynomials
        for (long k : range(d))
          conv(entry1[k], entry[k]);

        // compute the linearized polynomial coefficients
        ea.buildLinPolyCoeffs(diag[j], entry1);
      }
    }
    if (zDiag)
      return true; // zero diagonal, nothing to do

    // clear trailing zero entries
    for (long jj : range(nzLast + 1, nslots))
      diag[jj].assign(d, RX());

    // transpose and encode diag to form polys

    std::vector<RX> slots(nslots);
    poly.resize(d);
    for (long i : range(d)) {
      for (long j : range(nslots))
        slots[j] = diag[j][i];
      ea.encode(poly[i], slots);
    }

    return false; // a nonzero diagonal
  }

  // return true if zero
  static bool processDiagonal(std::vector<RX>& poly,
                              long i,
                              const EncryptedArrayDerived<type>& ea,
                              const BlockMatMul1D_derived<type>& mat)
  {
    if (mat.multipleTransforms())
      return processDiagonal2(poly, i, ea, mat);
    else
      return processDiagonal1(poly, i, ea, mat);
  }
};

template <typename type>
bool BlockMatMul1D_derived<type>::processDiagonal(
    std::vector<RX>& poly,
    long i,
    const EncryptedArrayDerived<type>& ea) const
{
  return BlockMatMul1D_derived_impl<type>::processDiagonal(poly, i, ea, *this);
}

// explicit instantiations
template bool BlockMatMul1D_derived<PA_GF2>::processDiagonal(
    std::vector<RX>& poly,
    long i,
    const EncryptedArrayDerived<PA_GF2>& ea) const;

template bool BlockMatMul1D_derived<PA_zz_p>::processDiagonal(
    std::vector<RX>& poly,
    long i,
    const EncryptedArrayDerived<PA_zz_p>& ea) const;

template <typename type>
struct BlockMatMul1DExec_construct
{
  PA_INJECT(type)

  // Basic logic:
  // We are computing \sum_i \sum_j c_{ij} \sigma^j rot^i(v),
  // where \sigma = frobenius, rot = rotation
  // For good dimensions, rot = \rho (the basic automorphism),
  // so we need to compute
  //     \sum_i \sum_j c_{ij} \sigma^j \rho^i(v)
  //  =  \sum_j \sigma^j[ \sigma^{-j}(c_{ij}) \rho^i(v) ]

  // For bad dimensions, we have
  //   rot^i(v) = d_i \rho^i(v) + e_i \rho^{i-D}(v)
  // and so we need to compute
  //   \sum_i \sum_j c_{ij} \sigma^j (d_i \rho^i(v) + e_i \rho^{i-D}(v))
  // =      \sum_j \sigma_j[  \sigma^{-j}(c_{ij}) d_i \rho^i(v) ] +
  //   \rho^{-D}[ \sum_j \sigma_j[ \rho^{D}{\sigma^{-j}(c_{ij}) e_i} \rho^i(v) ]
  //   ]

  // strategy == +1 : factor \sigma
  // strategy == -1 : factor \rho

  static void apply(const EncryptedArrayDerived<type>& ea,
                    const BlockMatMul1D& mat_basetype,
                    std::vector<std::shared_ptr<ConstMultiplier>>& vec,
                    std::vector<std::shared_ptr<ConstMultiplier>>& vec1,
                    long strategy)
  {
    const BlockMatMul1D_partial<type>& mat =
        dynamic_cast<const BlockMatMul1D_partial<type>&>(mat_basetype);

    long dim = mat.getDim();
    long D = dimSz(ea, dim);
    long d = ea.getDegree();
    bool native = dimNative(ea, dim);

    RBak bak;
    bak.save();
    ea.getTab().restoreContext();

    std::vector<RX> poly;

    switch (strategy) {
    case +1: // factor \sigma

      if (native) {
        vec.resize(D * d);
        for (long i : range(D)) {
          bool zero = mat.processDiagonal(poly, i, ea);
          if (zero) {
            for (long j : range(d))
              vec[i * d + j] = nullptr;
          } else {
            for (long j : range(d)) {
              vec[i * d + j] = build_ConstMultiplier(poly[j], -1, -j, ea);
            }
          }
        }
      } else {
        vec.resize(D * d);
        vec1.resize(D * d);
        for (long i : range(D)) {
          bool zero = mat.processDiagonal(poly, i, ea);
          if (zero) {
            for (long j : range(d)) {
              vec[i * d + j] = nullptr;
              vec1[i * d + j] = nullptr;
            }
          } else {
            const RX& mask = ea.getTab().getMaskTable()[dim][i];
            const RXModulus& F = ea.getTab().getPhimXMod();

            for (long j : range(d)) {
              plaintextAutomorph(poly[j], poly[j], -1, -j, ea);

              RX poly1;
              MulMod(poly1,
                     poly[j],
                     mask,
                     F); // poly[j] w/ first i slots zeroed out
              vec[i * d + j] = build_ConstMultiplier(poly1);

              sub(poly1,
                  poly[j],
                  poly1); // poly[j] w/ last D-i slots zeroed out
              vec1[i * d + j] = build_ConstMultiplier(poly1, dim, D, ea);
            }
          }
        }
      }
      break;

    case -1: // factor \rho

      if (native) {
        vec.resize(D * d);
        for (long i : range(D)) {
          bool zero = mat.processDiagonal(poly, i, ea);
          if (zero) {
            for (long j : range(d))
              vec[i + j * D] = nullptr;
          } else {
            for (long j : range(d)) {
              vec[i + j * D] = build_ConstMultiplier(poly[j], dim, -i, ea);
            }
          }
        }
      } else {
        vec.resize(D * d);
        vec1.resize(D * d);
        for (long i : range(D)) {
          bool zero = mat.processDiagonal(poly, i, ea);
          if (zero) {
            for (long j : range(d)) {
              vec[i + j * D] = nullptr;
              vec1[i + j * D] = nullptr;
            }
          } else {
            const RX& mask = ea.getTab().getMaskTable()[dim][i];
            const RXModulus& F = ea.getTab().getPhimXMod();

            for (long j : range(d)) {
              RX poly1, poly2;
              MulMod(poly1,
                     poly[j],
                     mask,
                     F); // poly[j] w/ first i slots zeroed out
              sub(poly2,
                  poly[j],
                  poly1); // poly[j] w/ last D-i slots zeroed out

              vec[i + j * D] = build_ConstMultiplier(poly1, dim, -i, ea);
              vec1[i + j * D] = build_ConstMultiplier(poly2, dim, D - i, ea);
            }
          }
        }
      }

      break;

    default:
      throw InvalidArgument("Unknown strategy");
    }
  }
};

HELIB_NO_CKKS_IMPL(BlockMatMul1DExec_construct)

BlockMatMul1DExec::BlockMatMul1DExec(const BlockMatMul1D& mat,
                                     UNUSED bool minimal) :
    ea(mat.getEA())
{
  HELIB_TIMER_START;

  dim = mat.getDim();
  assertInRange(dim,
                0l,
                ea.dimension(),
                "Matrix dimension not in [0, ea.dimension()]",
                true);

  // VJS-FIXME: we actually do use dim==ea.dimension() in bootstrapping.
  // We should document this feature, at least for internal purposes.
  // The basic idea is that this dimension is treated as a dimension
  // of size 1.
  // if (dim == ea.dimension()) std::cerr << "*** BlockMatMul1DExec: big dim\n";

  D = dimSz(ea, dim);
  d = ea.getDegree();
  native = dimNative(ea, dim);

  if (D >= d)
    strategy = +1;
  else
    strategy = -1;

  ea.dispatch<BlockMatMul1DExec_construct>(mat,
                                           cache.multiplier,
                                           cache1.multiplier,
                                           strategy);
}

void BlockMatMul1DExec::mul(Ctxt& ctxt) const
{
  HELIB_NTIMER_START(mul_BlockMatMul1DExec);
  assertEq(&ea.getContext(),
           &ctxt.getContext(),
           "Cannot multiply ciphertexts with context different to "
           "encrypted array one");
  const PAlgebra& zMStar = ea.getPAlgebra();

  ctxt.cleanUp();

  if (strategy == 0) {
    // assumes minimal KS matrices present

    if (native) {
      Ctxt acc(ZeroCtxtLike, ctxt);
      Ctxt sh_ctxt(ctxt);

      for (long i : range(D)) {
        if (i > 0)
          sh_ctxt.smartAutomorph(zMStar.genToPow(dim, 1));
        Ctxt sh_ctxt1(sh_ctxt);

        for (long j : range(d)) {
          if (j > 0)
            sh_ctxt1.smartAutomorph(zMStar.genToPow(-1, 1));
          MulAdd(acc, cache.multiplier[i * d + j], sh_ctxt1);
        }
      }

      ctxt = acc;
    } else {
      Ctxt acc(ZeroCtxtLike, ctxt);
      Ctxt acc1(ZeroCtxtLike, ctxt);
      Ctxt sh_ctxt(ctxt);

      for (long i : range(D)) {
        if (i > 0)
          sh_ctxt.smartAutomorph(zMStar.genToPow(dim, 1));
        Ctxt sh_ctxt1(sh_ctxt);

        for (long j : range(d)) {
          if (j > 0)
            sh_ctxt1.smartAutomorph(zMStar.genToPow(-1, 1));
          MulAdd(acc, cache.multiplier[i * d + j], sh_ctxt1);
          MulAdd(acc1, cache1.multiplier[i * d + j], sh_ctxt1);
        }
      }

      acc1.smartAutomorph(zMStar.genToPow(dim, -D));
      acc += acc1;
      ctxt = acc;
    }

    return;
  }

  long d0, d1;
  long dim0, dim1;

  if (strategy == +1) {
    d0 = D;
    dim0 = dim;
    d1 = d;
    dim1 = -1;
  } else {
    d1 = D;
    dim1 = dim;
    d0 = d;
    dim0 = -1;
  }

  const long par_buf_max = 50;

  bool iterative0 = false;
  if (ctxt.getPubKey().getKSStrategy(dim0) == HELIB_KSS_MIN)
    iterative0 = true;

  bool iterative1 = false;
  if (ctxt.getPubKey().getKSStrategy(dim1) == HELIB_KSS_MIN)
    iterative1 = true;
  if (ctxt.getPubKey().getKSStrategy(dim1) != HELIB_KSS_FULL &&
      NTL::AvailableThreads() == 1)
    iterative1 = true;

  if (native) {

    std::vector<Ctxt> acc(d1, Ctxt(ZeroCtxtLike, ctxt));

    if (iterative0) {
      Ctxt sh_ctxt(ctxt);

      for (long i : range(d0)) {
        if (i > 0) {
          sh_ctxt.smartAutomorph(zMStar.genToPow(dim0, 1));
          sh_ctxt.cleanUp();
        }
        for (long j : range(d1)) {
          MulAdd(acc[j], cache.multiplier[i * d1 + j], sh_ctxt);
        }
      }
    } else {

      std::shared_ptr<GeneralAutomorphPrecon> precon =
          buildGeneralAutomorphPrecon(ctxt, dim0, ea);

      long par_buf_sz = 1;
      if (NTL::AvailableThreads() > 1)
        par_buf_sz = std::min(d0, par_buf_max);

      std::vector<std::shared_ptr<Ctxt>> par_buf(par_buf_sz);

      for (long first_i = 0; first_i < d0; first_i += par_buf_sz) {
        long last_i = std::min(first_i + par_buf_sz, d0);

        // for i in [first_i..last_i), generate automorphism i and store
        // in par_buf[i-first_i]

        NTL_EXEC_RANGE(last_i - first_i, first, last)

        for (long idx : range(first, last)) {
          long i = idx + first_i;
          par_buf[idx] = precon->automorph(i);
        }

        NTL_EXEC_RANGE_END

        NTL_EXEC_RANGE(d1, first, last)

        for (long j : range(first, last)) {
          for (long i : range(first_i, last_i)) {
            MulAdd(acc[j], cache.multiplier[i * d1 + j], *par_buf[i - first_i]);
          }
        }

        NTL_EXEC_RANGE_END
      }
    }

    if (iterative1) {

      Ctxt sum(acc[d1 - 1]);
      for (long j = d1 - 2; j >= 0; j--) {
        sum.smartAutomorph(zMStar.genToPow(dim1, 1));
        sum.cleanUp();
        sum += acc[j];
      }

      ctxt = sum;

    } else {

      NTL::PartitionInfo pinfo(d1);
      long cnt = pinfo.NumIntervals();

      std::vector<Ctxt> sum(cnt, Ctxt(ZeroCtxtLike, ctxt));

      // for j in [0..d1)
      NTL_EXEC_INDEX(cnt, index)
      long first, last;
      pinfo.interval(first, last, index);
      for (long j : range(first, last)) {
        if (j > 0)
          acc[j].smartAutomorph(zMStar.genToPow(dim1, j));
        sum[index] += acc[j];
      }
      NTL_EXEC_INDEX_END

      ctxt = sum[0];
      for (long i : range(1, cnt))
        ctxt += sum[i];
    }
  } else {

    std::vector<Ctxt> acc(d1, Ctxt(ZeroCtxtLike, ctxt));
    std::vector<Ctxt> acc1(d1, Ctxt(ZeroCtxtLike, ctxt));

    if (iterative0) {
      Ctxt sh_ctxt(ctxt);

      for (long i : range(d0)) {
        if (i > 0) {
          sh_ctxt.smartAutomorph(zMStar.genToPow(dim0, 1));
          sh_ctxt.cleanUp();
        }
        for (long j : range(d1)) {
          MulAdd(acc[j], cache.multiplier[i * d1 + j], sh_ctxt);
          MulAdd(acc1[j], cache1.multiplier[i * d1 + j], sh_ctxt);
        }
      }
    } else {

      std::shared_ptr<GeneralAutomorphPrecon> precon =
          buildGeneralAutomorphPrecon(ctxt, dim0, ea);

      long par_buf_sz = 1;
      if (NTL::AvailableThreads() > 1)
        par_buf_sz = std::min(d0, par_buf_max);

      std::vector<std::shared_ptr<Ctxt>> par_buf(par_buf_sz);

      for (long first_i = 0; first_i < d0; first_i += par_buf_sz) {
        long last_i = std::min(first_i + par_buf_sz, d0);

        // for i in [first_i..last_i), generate automorphism i and store
        // in par_buf[i-first_i]

        NTL_EXEC_RANGE(last_i - first_i, first, last)

        for (long idx : range(first, last)) {
          long i = idx + first_i;
          par_buf[idx] = precon->automorph(i);
        }

        NTL_EXEC_RANGE_END

        NTL_EXEC_RANGE(d1, first, last)

        for (long j : range(first, last)) {
          for (long i : range(first_i, last_i)) {
            MulAdd(acc[j], cache.multiplier[i * d1 + j], *par_buf[i - first_i]);
            MulAdd(acc1[j],
                   cache1.multiplier[i * d1 + j],
                   *par_buf[i - first_i]);
          }
        }

        NTL_EXEC_RANGE_END
      }
    }

    if (iterative1) {

      Ctxt sum(acc[d1 - 1]);
      Ctxt sum1(acc1[d1 - 1]);

      for (long j = d1 - 2; j >= 0; j--) {
        sum.smartAutomorph(zMStar.genToPow(dim1, 1));
        sum.cleanUp();
        sum += acc[j];
        sum1.smartAutomorph(zMStar.genToPow(dim1, 1));
        sum1.cleanUp();
        sum1 += acc1[j];
      }

      sum1.smartAutomorph(zMStar.genToPow(dim, -D));
      ctxt = sum;
      ctxt += sum1;
    } else {

      NTL::PartitionInfo pinfo(d1);
      long cnt = pinfo.NumIntervals();

      std::vector<Ctxt> sum(cnt, Ctxt(ZeroCtxtLike, ctxt));
      std::vector<Ctxt> sum1(cnt, Ctxt(ZeroCtxtLike, ctxt));

      // for j in [0..d1)
      NTL_EXEC_INDEX(cnt, index)
      long first, last;
      pinfo.interval(first, last, index);
      for (long j : range(first, last)) {
        if (j > 0) {
          acc[j].smartAutomorph(zMStar.genToPow(dim1, j));
          acc1[j].smartAutomorph(zMStar.genToPow(dim1, j));
        }
        sum[index] += acc[j];
        sum1[index] += acc1[j];
      }
      NTL_EXEC_INDEX_END

      for (long i : range(1, cnt))
        sum[0] += sum[i];
      for (long i : range(1, cnt))
        sum1[0] += sum1[i];
      sum1[0].smartAutomorph(zMStar.genToPow(dim, -D));
      ctxt = sum[0];
      ctxt += sum1[0];
    }
  }
}

// ===================== MatMulFull stuff ==================

template <typename type>
class MatMulFullHelper : public MatMul1D_partial<type>
{
public:
  PA_INJECT(type)

  const EncryptedArray& ea_basetype;
  const MatMulFull_derived<type>& mat;
  std::vector<long> init_idxes;
  long dim;

  MatMulFullHelper(const EncryptedArray& _ea_basetype,
                   const MatMulFull_derived<type>& _mat,
                   const std::vector<long>& _init_idxes,
                   long _dim) :
      ea_basetype(_ea_basetype), mat(_mat), init_idxes(_init_idxes), dim(_dim)
  {}

  void processDiagonal(RX& epmat,
                       long offset,
                       const EncryptedArrayDerived<type>& ea) const override
  {
    std::vector<long> idxes;
    ea.EncryptedArrayBase::rotate1D(idxes, init_idxes, dim, offset);

    std::vector<RX> pmat; // the plaintext diagonal
    pmat.resize(ea.size());
    bool zDiag = true; // is this a zero diagonal
    for (long j : range(ea.size())) {
      long i = idxes[j];
      RX val;
      if (mat.get(val, i, j)) // returns true if the entry is zero
        clear(pmat[j]);
      else { // not a zero entry
        pmat[j] = val;
        zDiag = false; // not a zero diagonal
      }
    }
    // Now we have the constants for all the diagonal entries, encode the
    // diagonal as a single polynomial with these constants in the slots
    if (!zDiag)
      ea.encode(epmat, pmat);
    else
      epmat = 0;
  }

  const EncryptedArray& getEA() const override { return ea_basetype; }
  long getDim() const override { return dim; }
};

template <typename type>
struct MatMulFullExec_construct
{
  PA_INJECT(type)

  static long rec_mul(long dim,
                      long idx,
                      const std::vector<long>& idxes,
                      std::vector<MatMul1DExec>& transforms,
                      bool minimal,
                      const std::vector<long>& dims,
                      const EncryptedArray& ea_basetype,
                      const EncryptedArrayDerived<type>& ea,
                      const MatMulFull_derived<type>& mat)
  {
    if (dim >= ea.dimension() - 1) {
      // Last dimension (recursion edge condition)

      MatMulFullHelper<type> helper(ea_basetype, mat, idxes, dims[dim]);
      transforms.emplace_back(helper, minimal);
      idx++;
      return idx;
    }

    // not the last dimension, make a recursive call
    long sdim = ea.sizeOfDimension(dims[dim]);

    // compute "in spirit" sum_i (pdata >> i) * i'th-diagonal, but
    // adjust the indexes so that we only need to rotate the ciphertext
    // along the different dimensions separately
    for (long offset : range(sdim)) {
      std::vector<long> idxes1;
      ea.EncryptedArrayBase::rotate1D(idxes1, idxes, dims[dim], offset);
      idx = rec_mul(dim + 1,
                    idx,
                    idxes1,
                    transforms,
                    minimal,
                    dims,
                    ea_basetype,
                    ea,
                    mat);
    }

    return idx;
  }

  // helper class to sort dimensions, so that
  //  - small dimensions come before large dimensions
  //  - we break ties by putting good dimensions before
  //    bad dimensions...by doing so, we may save on
  //    some noise if the last dimension is bad (as it gets
  //    processed by MatMul1D).

  struct MatMulDimComp
  {
    const EncryptedArrayDerived<type>* ea;
    MatMulDimComp(const EncryptedArrayDerived<type>* _ea) : ea(_ea) {}

    bool operator()(long i, long j)
    {
      long si = ea->sizeOfDimension(i);
      bool ni = ea->nativeDimension(i);
      long sj = ea->sizeOfDimension(j);
      bool nj = ea->nativeDimension(j);

      return (si < sj) || ((si == sj) && ni && !nj);
    }
  };

  static void apply(const EncryptedArrayDerived<type>& ea,
                    const EncryptedArray& ea_basetype,
                    const MatMulFull& mat_basetype,
                    std::vector<MatMul1DExec>& transforms,
                    bool minimal,
                    std::vector<long>& dims)
  {
    const MatMulFull_derived<type>& mat =
        dynamic_cast<const MatMulFull_derived<type>&>(mat_basetype);

    long nslots = ea.size();
    long ndims = ea.dimension();

    RBak bak;
    bak.save();
    ea.getTab().restoreContext();

    dims.resize(ndims);
    for (long i : range(ndims))
      dims[i] = i;
    sort(dims.begin(), dims.end(), MatMulDimComp(&ea));

    std::vector<long> idxes(nslots);
    for (long i : range(nslots))
      idxes[i] = i;

    rec_mul(0, 0, idxes, transforms, minimal, dims, ea_basetype, ea, mat);
  }
};

HELIB_NO_CKKS_IMPL(MatMulFullExec_construct)

MatMulFullExec::MatMulFullExec(const MatMulFull& mat, bool _minimal) :
    ea(mat.getEA()), minimal(_minimal)
{
  HELIB_NTIMER_START(MatMulFullExec);

  ea.dispatch<MatMulFullExec_construct>(ea, mat, transforms, minimal, dims);
}

long MatMulFullExec::rec_mul(Ctxt& acc,
                             const Ctxt& ctxt,
                             long dim_idx,
                             long idx) const
{
  if (dim_idx >= ea.dimension() - 1) {
    // Last dimension (recursion edge condition)

    Ctxt tmp = ctxt;
    transforms[idx].mul(tmp);
    acc += tmp;

    idx++;

  } else {
    long dim = dims[dim_idx];
    long sdim = ea.sizeOfDimension(dim);
    bool native = ea.nativeDimension(dim);
    const PAlgebra& zMStar = ea.getPAlgebra();

    bool iterative = false;
    if (ctxt.getPubKey().getKSStrategy(dim) == HELIB_KSS_MIN)
      iterative = true;

    if (!iterative) {

      if (native) {
        std::shared_ptr<GeneralAutomorphPrecon> precon =
            buildGeneralAutomorphPrecon(ctxt, dim, ea);

        for (long i : range(sdim)) {
          std::shared_ptr<Ctxt> tmp = precon->automorph(i);
          idx = rec_mul(acc, *tmp, dim_idx + 1, idx);
        }
      } else {
        Ctxt ctxt1 = ctxt;
        ctxt1.smartAutomorph(zMStar.genToPow(dim, -sdim));
        std::shared_ptr<GeneralAutomorphPrecon> precon =
            buildGeneralAutomorphPrecon(ctxt, dim, ea);
        std::shared_ptr<GeneralAutomorphPrecon> precon1 =
            buildGeneralAutomorphPrecon(ctxt1, dim, ea);

        for (long i : range(sdim)) {
          if (i == 0)
            idx = rec_mul(acc, ctxt, dim_idx + 1, idx);
          else {
            std::shared_ptr<Ctxt> tmp = precon->automorph(i);
            std::shared_ptr<Ctxt> tmp1 = precon1->automorph(i);

            zzX mask = ea.getAlMod().getMask_zzX(dim, i);
            double sz = embeddingLargestCoeff(mask, zMStar);

            DoubleCRT m1(mask,
                         ea.getContext(),
                         tmp->getPrimeSet() | tmp1->getPrimeSet());

            // Compute tmp = tmp*m1 + tmp1 - tmp1*m1
            tmp->multByConstant(m1, sz);
            *tmp += *tmp1;
            tmp1->multByConstant(m1, sz);
            *tmp -= *tmp1;

            idx = rec_mul(acc, *tmp, dim_idx + 1, idx);
          }
        }
      }

    } else {

      if (native) {
        Ctxt sh_ctxt = ctxt;
        for (long offset : range(sdim)) {
          if (offset > 0)
            sh_ctxt.smartAutomorph(zMStar.genToPow(dim, 1));
          idx = rec_mul(acc, sh_ctxt, dim_idx + 1, idx);
        }
      } else {
        Ctxt sh_ctxt = ctxt;
        Ctxt sh_ctxt1 = ctxt;
        sh_ctxt1.smartAutomorph(zMStar.genToPow(dim, -sdim));

        for (long offset : range(sdim)) {
          if (offset == 0)
            idx = rec_mul(acc, ctxt, dim_idx + 1, idx);
          else {
            sh_ctxt.smartAutomorph(zMStar.genToPow(dim, 1));
            sh_ctxt1.smartAutomorph(zMStar.genToPow(dim, 1));

            zzX mask = ea.getAlMod().getMask_zzX(dim, offset);
            double sz = embeddingLargestCoeff(mask, zMStar);

            Ctxt tmp = sh_ctxt;
            Ctxt tmp1 = sh_ctxt1;

            DoubleCRT m1(mask,
                         ea.getContext(),
                         tmp.getPrimeSet() | tmp1.getPrimeSet());

            // Compute tmp = tmp*m1 + tmp1 - tmp1*m1
            tmp.multByConstant(m1, sz);
            tmp += tmp1;
            tmp1.multByConstant(m1, sz);
            tmp -= tmp1;

            idx = rec_mul(acc, tmp, dim_idx + 1, idx);
          }
        }
      }
    }
  }

  return idx;
}

void MatMulFullExec::mul(Ctxt& ctxt) const
{
  HELIB_NTIMER_START(mul_MatMulFullExec);
  assertEq(&ea.getContext(),
           &ctxt.getContext(),
           "Cannot multiply ciphertexts with context different to "
           "encrypted array one");

  assertTrue(ea.size() > 1l, "Number of slots is less than 2");
  // FIXME: right now, the code does not work if ea.size() == 1
  // (which means that # dimensions == 0).  This is a corner case
  // that is hardly worth dealing with (although we could).

  ctxt.cleanUp();

  Ctxt acc(ZeroCtxtLike, ctxt);
  rec_mul(acc, ctxt, 0, 0);

  ctxt = acc;
}

// ================= BlockMatMulFull stuff ===============

// lightly massaged version of MatMulFull code...some unfortunate
// code duplication here

template <typename type>
class BlockMatMulFullHelper : public BlockMatMul1D_partial<type>
{
public:
  PA_INJECT(type)

  const EncryptedArray& ea_basetype;
  const BlockMatMulFull_derived<type>& mat;
  std::vector<long> init_idxes;
  long dim;

  BlockMatMulFullHelper(const EncryptedArray& _ea_basetype,
                        const BlockMatMulFull_derived<type>& _mat,
                        const std::vector<long>& _init_idxes,
                        long _dim) :
      ea_basetype(_ea_basetype), mat(_mat), init_idxes(_init_idxes), dim(_dim)
  {}

  bool processDiagonal(std::vector<RX>& poly,
                       long offset,
                       const EncryptedArrayDerived<type>& ea) const override
  {
    std::vector<long> idxes;
    ea.EncryptedArrayBase::rotate1D(idxes, init_idxes, dim, offset);

    long d = ea.getDegree();
    long nslots = ea.size();
    bool zDiag = true; // is this a zero diagonal?
    long nzLast = -1;  // index of last non-zero entry

    mat_R entry(NTL::INIT_SIZE, d, d);
    std::vector<RX> entry1(d);

    std::vector<std::vector<RX>> diag(nslots);

    for (long j : range(nslots)) {
      long i = idxes[j];
      bool zEntry = mat.get(entry, i, j);

      // the remainder is copied and pasted from the processDiagonal2 logic
      // for BlockMatMul1D....FIXME: code duplication

      if (!zEntry && IsZero(entry))
        zEntry = true; // zero is an empty entry too
      assertTrue(zEntry || (entry.NumRows() == d && entry.NumCols() == d),
                 "Non zero entry and number of entry rows and columns "
                 "are not equal to d");

      if (!zEntry) {   // non-empty entry
        zDiag = false; // mark diagonal as non-empty

        for (long jj : range(nzLast + 1, j)) // clear from last nonzero entry
          diag[jj].assign(d, RX());

        nzLast = j; // current entry is the last nonzero one

        // recode entry as a vector of polynomials
        for (long k : range(d))
          conv(entry1[k], entry[k]);

        // compute the linearized polynomial coefficients
        ea.buildLinPolyCoeffs(diag[j], entry1);
      }
    }
    if (zDiag)
      return true; // zero diagonal, nothing to do

    // clear trailing zero entries
    for (long jj : range(nzLast + 1, nslots))
      diag[jj].assign(d, RX());

    // transpose and encode diag to form polys

    std::vector<RX> slots(nslots);
    poly.resize(d);
    for (long i : range(d)) {
      for (long j : range(nslots))
        slots[j] = diag[j][i];
      ea.encode(poly[i], slots);
    }

    return false; // a nonzero diagonal
  }

  const EncryptedArray& getEA() const override { return ea_basetype; }
  long getDim() const override { return dim; }
};

template <typename type>
struct BlockMatMulFullExec_construct
{
  PA_INJECT(type)

  static long rec_mul(long dim,
                      long idx,
                      const std::vector<long>& idxes,
                      std::vector<BlockMatMul1DExec>& transforms,
                      bool minimal,
                      const std::vector<long>& dims,
                      const EncryptedArray& ea_basetype,
                      const EncryptedArrayDerived<type>& ea,
                      const BlockMatMulFull_derived<type>& mat)
  {
    if (dim >= ea.dimension() - 1) {
      // Last dimension (recursion edge condition)

      BlockMatMulFullHelper<type> helper(ea_basetype, mat, idxes, dims[dim]);
      transforms.emplace_back(helper, minimal);
      idx++;
      return idx;
    }

    // not the last dimension, make a recursive call
    long sdim = ea.sizeOfDimension(dims[dim]);

    // compute "in spirit" sum_i (pdata >> i) * i'th-diagonal, but
    // adjust the indexes so that we only need to rotate the ciphertext
    // along the different dimensions separately
    for (long offset : range(sdim)) {
      std::vector<long> idxes1;
      ea.EncryptedArrayBase::rotate1D(idxes1, idxes, dims[dim], offset);
      idx = rec_mul(dim + 1,
                    idx,
                    idxes1,
                    transforms,
                    minimal,
                    dims,
                    ea_basetype,
                    ea,
                    mat);
    }

    return idx;
  }

  // helper class to sort dimensions, so that
  //  - small dimensions come before large dimensions
  //  - we break ties by putting good dimensions before
  //    bad dimensions...by doing so, we may save on
  //    some noise if the last dimension is bad (as it gets
  //    processed by BlockMatMul1D).

  struct BlockMatMulDimComp
  {
    const EncryptedArrayDerived<type>* ea;
    BlockMatMulDimComp(const EncryptedArrayDerived<type>* _ea) : ea(_ea) {}

    bool operator()(long i, long j)
    {
      long si = ea->sizeOfDimension(i);
      bool ni = ea->nativeDimension(i);
      long sj = ea->sizeOfDimension(j);
      bool nj = ea->nativeDimension(j);

      return (si < sj) || ((si == sj) && ni && !nj);
    }
  };

  static void apply(const EncryptedArrayDerived<type>& ea,
                    const EncryptedArray& ea_basetype,
                    const BlockMatMulFull& mat_basetype,
                    std::vector<BlockMatMul1DExec>& transforms,
                    bool minimal,
                    std::vector<long>& dims)
  {
    const BlockMatMulFull_derived<type>& mat =
        dynamic_cast<const BlockMatMulFull_derived<type>&>(mat_basetype);

    long nslots = ea.size();
    long ndims = ea.dimension();

    RBak bak;
    bak.save();
    ea.getTab().restoreContext();

    dims.resize(ndims);
    for (long i : range(ndims))
      dims[i] = i;
    sort(dims.begin(), dims.end(), BlockMatMulDimComp(&ea));

    std::vector<long> idxes(nslots);
    for (long i : range(nslots))
      idxes[i] = i;

    rec_mul(0, 0, idxes, transforms, minimal, dims, ea_basetype, ea, mat);
  }
};

HELIB_NO_CKKS_IMPL(BlockMatMulFullExec_construct)

BlockMatMulFullExec::BlockMatMulFullExec(const BlockMatMulFull& mat,
                                         bool _minimal) :
    ea(mat.getEA()), minimal(_minimal)
{
  HELIB_NTIMER_START(BlockMatMulFullExec);

  ea.dispatch<BlockMatMulFullExec_construct>(ea,
                                             mat,
                                             transforms,
                                             minimal,
                                             dims);
}

long BlockMatMulFullExec::rec_mul(Ctxt& acc,
                                  const Ctxt& ctxt,
                                  long dim_idx,
                                  long idx) const
{
  if (dim_idx >= ea.dimension() - 1) {
    // Last dimension (recursion edge condition)

    Ctxt tmp = ctxt;
    transforms[idx].mul(tmp);
    acc += tmp;

    idx++;

  } else {
    long dim = dims[dim_idx];
    long sdim = ea.sizeOfDimension(dim);
    bool native = ea.nativeDimension(dim);
    const PAlgebra& zMStar = ea.getPAlgebra();

    bool iterative = false;
    if (ctxt.getPubKey().getKSStrategy(dim) == HELIB_KSS_MIN)
      iterative = true;

    if (!iterative) {

      if (native) {
        std::shared_ptr<GeneralAutomorphPrecon> precon =
            buildGeneralAutomorphPrecon(ctxt, dim, ea);

        for (long i : range(sdim)) {
          std::shared_ptr<Ctxt> tmp = precon->automorph(i);
          idx = rec_mul(acc, *tmp, dim_idx + 1, idx);
        }
      } else {
        Ctxt ctxt1 = ctxt;
        ctxt1.smartAutomorph(zMStar.genToPow(dim, -sdim));
        std::shared_ptr<GeneralAutomorphPrecon> precon =
            buildGeneralAutomorphPrecon(ctxt, dim, ea);
        std::shared_ptr<GeneralAutomorphPrecon> precon1 =
            buildGeneralAutomorphPrecon(ctxt1, dim, ea);

        for (long i : range(sdim)) {
          if (i == 0)
            idx = rec_mul(acc, ctxt, dim_idx + 1, idx);
          else {
            std::shared_ptr<Ctxt> tmp = precon->automorph(i);
            std::shared_ptr<Ctxt> tmp1 = precon1->automorph(i);

            zzX mask = ea.getAlMod().getMask_zzX(dim, i);
            double sz = embeddingLargestCoeff(mask, zMStar);

            DoubleCRT m1(mask,
                         ea.getContext(),
                         tmp->getPrimeSet() | tmp1->getPrimeSet());

            // Compute tmp = tmp*m1 + tmp1 - tmp1*m1
            tmp->multByConstant(m1, sz);
            *tmp += *tmp1;
            tmp1->multByConstant(m1, sz);
            *tmp -= *tmp1;

            idx = rec_mul(acc, *tmp, dim_idx + 1, idx);
          }
        }
      }

    } else {

      if (native) {
        Ctxt sh_ctxt = ctxt;
        for (long offset : range(sdim)) {
          if (offset > 0)
            sh_ctxt.smartAutomorph(zMStar.genToPow(dim, 1));
          idx = rec_mul(acc, sh_ctxt, dim_idx + 1, idx);
        }
      } else {
        Ctxt sh_ctxt = ctxt;
        Ctxt sh_ctxt1 = ctxt;
        sh_ctxt1.smartAutomorph(zMStar.genToPow(dim, -sdim));

        for (long offset : range(sdim)) {
          if (offset == 0)
            idx = rec_mul(acc, ctxt, dim_idx + 1, idx);
          else {
            sh_ctxt.smartAutomorph(zMStar.genToPow(dim, 1));
            sh_ctxt1.smartAutomorph(zMStar.genToPow(dim, 1));

            zzX mask = ea.getAlMod().getMask_zzX(dim, offset);
            double sz = embeddingLargestCoeff(mask, zMStar);

            Ctxt tmp = sh_ctxt;
            Ctxt tmp1 = sh_ctxt1;

            DoubleCRT m1(mask,
                         ea.getContext(),
                         tmp.getPrimeSet() | tmp1.getPrimeSet());

            // Compute tmp = tmp*m1 + tmp1 - tmp1*m1
            tmp.multByConstant(m1, sz);
            tmp += tmp1;
            tmp1.multByConstant(m1, sz);
            tmp -= tmp1;

            idx = rec_mul(acc, tmp, dim_idx + 1, idx);
          }
        }
      }
    }
  }

  return idx;
}

void BlockMatMulFullExec::mul(Ctxt& ctxt) const
{
  HELIB_NTIMER_START(mul_BlockMatMulFullExec);
  assertEq(&ea.getContext(),
           &ctxt.getContext(),
           "Cannot multiply ciphertexts with context different to "
           "encrypted array one");

  assertTrue(ea.size() > 1l, "Number of slots is less than 2");
  // FIXME: right now, the code does not work if ea.size() == 1
  // (which means that # dimensions == 0).  This is a corner case
  // that is hardly worth dealing with (although we could).

  ctxt.cleanUp();

  Ctxt acc(ZeroCtxtLike, ctxt);
  rec_mul(acc, ctxt, 0, 0);

  ctxt = acc;
}

// ================= plaintext mul stuff stuff ===============

template <typename type>
struct mul_MatMul1D_impl
{
  PA_INJECT(type)

  static void apply(const EncryptedArrayDerived<type>& ea,
                    PlaintextArray& pa,
                    const MatMul1D& mat_basetype)
  {
    const MatMul1D_derived<type>& mat =
        dynamic_cast<const MatMul1D_derived<type>&>(mat_basetype);
    long dim = mat.getDim();

    RBak bak;
    bak.save();
    ea.getTab().restoreContext();

    long n = ea.size();
    long D = ea.sizeOfDimension(dim);

    std::vector<std::vector<RX>> data1(n / D);
    for (long k : range(n / D))
      data1[k].resize(D);

    // copy the data into a vector of 1D vectors
    std::vector<RX>& data = pa.getData<type>();
    for (long i : range(n)) {
      long k, j;
      std::tie(k, j) = ea.getContext().getZMStar().breakIndexByDim(i, dim);
      data1[k][j] = data[i]; // k= along dim, j = the rest of i
    }

    // multiply each one of the vectors by the same matrix
    for (long k : range(n / D)) {
      for (long j : range(D)) { // simple matrix-vector multiplication
        std::pair<long, long> p(k, j);
        long idx = ea.getContext().getZMStar().assembleIndexByDim(p, dim);

        RX acc, val, tmp;
        acc = 0;
        for (long i : range(D)) {
          bool zero = mat.get(val, i, j, k);
          if (!zero) {
            NTL::mul(tmp, data1[k][i], val);
            NTL::add(acc, acc, tmp);
          }
        }
        rem(data[idx], acc, ea.getG()); // store the result in the data array
      }
    }
  }
};

template <>
struct mul_MatMul1D_impl<PA_cx>
{
  PA_INJECT(PA_cx)

  static void apply(const EncryptedArrayDerived<PA_cx>& ea,
                    PlaintextArray& pa,
                    const MatMul1D& mat_basetype)
  {
    const MatMul1D_derived<PA_cx>& mat =
        dynamic_cast<const MatMul1D_derived<PA_cx>&>(mat_basetype);

    long n = ea.size();
    std::vector<std::complex<double>>& data = pa.getData<PA_cx>();
    std::vector<std::complex<double>> data1(n);

    // compute data1 = data*mat
    for (long i : range(n))
      for (long j : range(n))
        data1[j] += mat.get(i, j) * data[i];

    data = data1;
  }
};

void mul(PlaintextArray& pa, const MatMul1D& mat)
{
  const EncryptedArray& ea = mat.getEA();
  ea.dispatch<mul_MatMul1D_impl>(pa, mat);
}

template <typename type>
struct mul_BlockMatMul1D_impl
{
  PA_INJECT(type)

  static void apply(const EncryptedArrayDerived<type>& ea,
                    PlaintextArray& pa,
                    const BlockMatMul1D& mat_basetype)
  {
    const BlockMatMul1D_derived<type>& mat =
        dynamic_cast<const BlockMatMul1D_derived<type>&>(mat_basetype);
    const PAlgebra& zMStar = ea.getPAlgebra();
    long dim = mat.getDim();

    RBak bak;
    bak.save();
    ea.getTab().restoreContext();

    long n = ea.size();
    long D = ea.sizeOfDimension(dim);
    long d = ea.getDegree();

    std::vector<std::vector<RX>> data1(n / D);
    for (long k : range(n / D))
      data1[k].resize(D);

    // copy the data into a vector of 1D vectors
    std::vector<RX>& data = pa.getData<type>();
    for (long i : range(n)) {
      long k, j;
      std::tie(k, j) = zMStar.breakIndexByDim(i, dim);
      data1[k][j] = data[i]; // k= along dim, j = the rest of i
    }

    for (long k : range(n / D)) { // multiply each vector by a matrix
      for (long j : range(D)) {   // matrix-vector multiplication
        vec_R acc, tmp, tmp1;
        mat_R val;
        acc.SetLength(d);
        for (long i = 0; i < D; i++) {
          bool zero = mat.get(val, i, j, k);
          if (!zero) { // if non-zero, multiply and add
            VectorCopy(tmp1, data1[k][i], d);
            mul(tmp, tmp1, val);
            add(acc, acc, tmp);
          }
        }
        long idx = zMStar.assembleIndexByDim(std::make_pair(k, j), dim);
        conv(data[idx], acc);
      }
    }
  }
};

HELIB_NO_CKKS_IMPL(mul_BlockMatMul1D_impl)

void mul(PlaintextArray& pa, const BlockMatMul1D& mat)
{
  const EncryptedArray& ea = mat.getEA();
  ea.dispatch<mul_BlockMatMul1D_impl>(pa, mat);
}

template <typename type>
struct mul_MatMulFull_impl
{
  PA_INJECT(type)

  static void apply(const EncryptedArrayDerived<type>& ea,
                    PlaintextArray& pa,
                    const MatMulFull& mat_basetype)
  {
    const MatMulFull_derived<type>& mat =
        dynamic_cast<const MatMulFull_derived<type>&>(mat_basetype);
    long n = ea.size();
    const RX& G = ea.getG();
    std::vector<RX>& data = pa.getData<type>();

    RBak bak;
    bak.save();
    ea.getTab().restoreContext();

    std::vector<RX> res;
    res.resize(n);
    for (long j : range(n)) {
      RX acc, val, tmp;
      acc = 0;
      for (long i : range(n)) {
        if (!mat.get(val, i, j)) {
          mul(tmp, data[i], val);
          add(acc, acc, tmp);
        }
      }
      rem(acc, acc, G);
      res[j] = acc;
    }

    data = res;
  }
};

HELIB_NO_CKKS_IMPL(mul_MatMulFull_impl)

void mul(PlaintextArray& pa, const MatMulFull& mat)
{
  const EncryptedArray& ea = mat.getEA();
  ea.dispatch<mul_MatMulFull_impl>(pa, mat);
}

template <typename type>
struct mul_BlockMatMulFull_impl
{
  PA_INJECT(type)

  static void apply(const EncryptedArrayDerived<type>& ea,
                    PlaintextArray& pa,
                    const BlockMatMulFull& mat_basetype)
  {
    const BlockMatMulFull_derived<type>& mat =
        dynamic_cast<const BlockMatMulFull_derived<type>&>(mat_basetype);
    long n = ea.size();
    long d = ea.getDegree();
    std::vector<RX>& data = pa.getData<type>();

    RBak bak;
    bak.save();
    ea.getTab().restoreContext();

    std::vector<RX> res;
    res.resize(n);
    for (long j : range(n)) {
      vec_R acc, tmp, tmp1;
      mat_R val;
      acc.SetLength(d);
      for (long i : range(n)) {
        if (!mat.get(val, i, j)) {
          VectorCopy(tmp1, data[i], d);
          mul(tmp, tmp1, val);
          add(acc, acc, tmp);
        }
      }
      conv(res[j], acc);
    }

    data = res;
  }
};

HELIB_NO_CKKS_IMPL(mul_BlockMatMulFull_impl)

void mul(PlaintextArray& pa, const BlockMatMulFull& mat)
{
  const EncryptedArray& ea = mat.getEA();
  ea.dispatch<mul_BlockMatMulFull_impl>(pa, mat);
}

//================= traceMap ====================

#define HELIB_TRACE_THRESH (50)
// This should probably just be the same as HELIB_KEYSWITCH_THRESH,
// but it can be adjusted.

void traceMap(Ctxt& ctxt)
{
  const Context& context = ctxt.getContext();
  const PAlgebra& zMStar = context.getZMStar();
  long d = context.getOrdP();

  if (d == 1)
    return;

  Ctxt orig = ctxt;

  long strategy = ctxt.getPubKey().getKSStrategy(-1);

  if (strategy == HELIB_KSS_FULL && d <= HELIB_TRACE_THRESH) {
    BasicAutomorphPrecon precon(ctxt);
    Ctxt acc(ctxt);

    for (long i : range(1, d)) {
      std::shared_ptr<Ctxt> tmp = precon.automorph(zMStar.genToPow(-1, i));
      acc += *tmp;
    }

    ctxt = acc;
  } else if (strategy == HELIB_KSS_MIN) {
    if (d <= HELIB_KEYSWITCH_MIN_THRESH) {
      // simple iterative procedure

      Ctxt acc(ctxt);
      for (long i = 1; i < d; ++i) {
        acc.frobeniusAutomorph(1);
        acc += ctxt;
      }
      ctxt = acc;
    } else {
      long g = KSGiantStepSize(d);
      long q = d / g;
      long r = d - g * q; // d = g*q + r

      if (r == 0) {
        // baby step / giant step w/ no remainder

        // compute baby_sum = sum_{i=0}^{g-1} \sigma^i(ctxt)
        Ctxt baby_sum(ctxt);
        for (long i = 1; i < g; ++i) {
          baby_sum.frobeniusAutomorph(1);
          baby_sum += ctxt;
        }

        Ctxt acc(baby_sum);
        for (long i = 1; i < q; ++i) {
          acc.frobeniusAutomorph(g);
          acc += baby_sum;
        }

        ctxt = acc;
      } else {
        // baby step / giant step w/ remainder

        // compute baby_sum = sum_{i=0}^{g-1} \sigma^i(ctxt)
        //   and store rem_sum = sum_{i=0}^{r-1} \sigma^i(ctxt)
        Ctxt baby_sum(ctxt);
        Ctxt rem_sum(ZeroCtxtLike, ctxt);
        for (long i : range(1, g)) {
          if (i == r)
            rem_sum = baby_sum;
          baby_sum.frobeniusAutomorph(1);
          baby_sum += ctxt;
        }

        Ctxt acc(rem_sum);
        for (long i = 0; i < q; ++i) {
          acc.frobeniusAutomorph(g);
          acc += baby_sum;
        }

        ctxt = acc;
      }
    }
  } else {

    long k = NTL::NumBits(d);
    long e = 1;

    for (long i = k - 2; i >= 0; i--) {
      Ctxt tmp1 = ctxt;
      tmp1.frobeniusAutomorph(e);
      ctxt += tmp1;
      e = 2 * e;

      if (NTL::bit(d, i)) {
        ctxt.frobeniusAutomorph(1);
        ctxt += orig;
        e += 1;
      }
    }
  }
}

} // namespace helib

#endif
