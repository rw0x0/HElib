#pragma once

#include <helib/keys.h>
#include <map>

namespace helib {

class GaloisKey2k {
  private:
    size_t m;
    std::map<size_t, KeySwitch> keys;

    //! Translates a step into a galois_elt
    size_t get_elt_from_step(int32_t step);

    //! The actual key switch after the automorphism
    void key_switch(Ctxt& ctxt, size_t galois_elt);

  public:
    GaloisKey2k(size_t m) : m(m) {
      if (m == 0 ||  (m & (m - 1)) != 0) { // check if power of 2
        throw RuntimeError("DoubleCRT::power_of_two_galois_automorph: Wrong configuration");
      }
    }
    virtual ~GaloisKey2k() = default;

    GaloisKey2k() = delete;
    GaloisKey2k(const GaloisKey2k& other) = delete;
    GaloisKey2k(GaloisKey2k&& other) = delete;
    GaloisKey2k& operator=(const GaloisKey2k&) = delete;

    //! Generate a key-switching matrix for a given step
    void generate_step(const SecKey& sKey, int32_t step);

    //! Apply the galois automorphism to a ciphertext, where step=0 implys rotate columns
    void rotate(Ctxt& ctxt, int32_t step);
};

} // namespace helib
