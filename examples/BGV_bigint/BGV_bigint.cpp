/* Copyright (C) 2019-2021 IBM Corp.
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

// This is a sample program for education purposes only.
// It attempts to show the various basic mathematical
// operations that can be performed on both ciphertexts
// and plaintexts.

#include <iostream>

#include <helib/helib.h>
#include <NTL/ZZ.h>

int main(int argc, char* argv[])
{
  int error =0;
  /*  Example of BGV scheme  */

  // Plaintext prime modulus
  NTL::ZZ p = NTL::to_ZZ("21888242871839275222246405745257275088548364400416034343698204186575808495617");
  // Cyclotomic polynomial - defines phi(m)
  unsigned long m = 32109;
  // Hensel lifting (default = 1)
  unsigned long r = 1;
  // Number of bits of the modulus chain
  unsigned long bits = 500;
  // Number of columns of Key-Switching matrix (default = 2 or 3)
  unsigned long c = 2;

  std::cout << "\n*********************************************************";
  std::cout << "\n*         Basic Mathematical Operations Example         *";
  std::cout << "\n*         =====================================         *";
  std::cout << "\n*                                                       *";
  std::cout << "\n* This is a sample program for education purposes only. *";
  std::cout << "\n* It attempts to show the various basic mathematical    *";
  std::cout << "\n* operations that can be performed on both ciphertexts  *";
  std::cout << "\n* and plaintexts.                                       *";
  std::cout << "\n*                                                       *";
  std::cout << "\n*********************************************************";
  std::cout << std::endl;

  std::cout << "Initialising context object..." << std::endl;
  // Initialize context
  // This object will hold information about the algebra created from the
  // previously set parameters
  helib::Context context = helib::ContextBuilder<helib::BGV>()
                               .m(m)
                               .p(p)
                               .r(r)
                               .bits(bits)
                               .c(c)
                               .build();

  // Print the context
  context.printout();
  std::cout << std::endl;

  // Secret key management
  std::cout << "Creating secret key..." << std::endl;
  // Create a secret key associated with the context
  helib::SecKey secret_key(context);
  // Generate the secret key
  secret_key.GenSecKey();

  // Public key management
  // Set the secret key (upcast: SecKey is a subclass of PubKey)
  std::cout << "Creating public key..." << std::endl;
  const helib::PubKey& public_key = secret_key;

  // Get two plaintexts
  std::cout << "Encrypting two plaintexts..." << std::endl;
  NTL::ZZ p1 = NTL::ZZ(5);
  NTL::ZZ p2 = NTL::ZZ(10);
  NTL::ZZX ptxt1 = NTL::ZZX(p1);
  NTL::ZZX ptxt2 = NTL::ZZX(p2);

  // Encrypt the two plaintexts
  helib::Ctxt ctxt1(public_key);
  public_key.Encrypt(ctxt1, ptxt1);

  helib::Ctxt ctxt2(public_key);
  public_key.Encrypt(ctxt2, ptxt2);

  std::cout << "Noise budget in ctxt1: " << ctxt1.bitCapacity() << std::endl;
  std::cout << "Noise budget in ctxt2: " << ctxt2.bitCapacity() << std::endl;

 {
    // Adding the ciphertexts
    std::cout << std::endl;
    std::cout << "Adding the two ciphertexts..." << std::endl;
    auto ctxt3 = ctxt1;
    ctxt3 += ctxt2;
    std::cout << "Noise budget in ctxt3: " << ctxt3.bitCapacity() << std::endl;

    // Decrypt the result
    std::cout << "Decrypting the result.." << std::endl;
    NTL::ZZX decrypted;
    secret_key.Decrypt(decrypted, ctxt3);

    // Compare the result
    NTL::ZZ p3 = (p1 + p2) % p;
    NTL::ZZ p3_is = NTL::conv<NTL::ZZ>(decrypted[0]);
    std::cout << "Decrypted result: " << p3_is << std::endl;
    if (p3 == p3_is)
        std::cout << "Decryption is correct!" << std::endl;
    else {
        std::cout << "Decryption is incorrect!" << std::endl;
        error = 1;
    }
  }

  {
    // Subtracting the ciphertexts
    std::cout << std::endl;
    std::cout << "Subtracting the two ciphertexts..." << std::endl;
    auto ctxt3 = ctxt1;
    ctxt3 -= ctxt2;
    std::cout << "Noise budget in ctxt3: " << ctxt3.bitCapacity() << std::endl;

    // Decrypt the result
    std::cout << "Decrypting the result.." << std::endl;
    NTL::ZZX decrypted;
    secret_key.Decrypt(decrypted, ctxt3);

    // Compare the result
    NTL::ZZ p3 = (p1 + p - p2) % p;
    NTL::ZZ p3_is = NTL::conv<NTL::ZZ>(decrypted[0]);
    std::cout << "Decrypted result: " << p3_is << std::endl;
    if (p3 == p3_is)
        std::cout << "Decryption is correct!" << std::endl;
    else {
        std::cout << "Decryption is incorrect!" << std::endl;
        error = 1;
    }
  }

  return error;
}
