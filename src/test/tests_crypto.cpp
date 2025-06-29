#include "catch2/catch_all.hpp"

#include "tbx/hash/hash.h"
#include "crypto/crypto.h"
#include "test_support.h"



TEST_CASE("algorithms.CryptoAES::ecb128")
{
  /* key, plain, cipher */
  const std::vector<std::tuple<std::string, std::string, std::string>> AES128_ECB_tests = {
    { "00000000000000000000000000000000", "f34481ec3cc627bacd5dc3fb08f273e6", "0336763e966d92595a567cc9ce537f5e" },
    { "00000000000000000000000000000000", "9798c4640bad75c7c3227db910174e72", "a9a1631bf4996954ebc093957b234589" },
    { "00000000000000000000000000000000", "96ab5c2ff612d9dfaae8c31f30c42168", "ff4f8391a6a40ca5b25d23bedd44a597" },
    { "00000000000000000000000000000000", "6a118a874519e64e9963798a503f1d35", "dc43be40be0e53712f7e2bf5ca707209" },
    { "00000000000000000000000000000000", "cb9fceec81286ca3e989bd979b0cb284", "92beedab1895a94faa69b632e5cc47ce" },
    { "00000000000000000000000000000000", "b26aeb1874e47ca8358ff22378f09144", "459264f4798f6a78bacb89c15ed3d601" },
    { "00000000000000000000000000000000", "58c8e00b2631686d54eab84b91f0aca1", "08a4e2efec8a8e3312ca7460b9040bbf" },
    { "80000000000000000000000000000000", "00000000000000000000000000000000", "0edd33d3c621e546455bd8ba1418bec8" },
    { "fffffffffffff0000000000000000000", "00000000000000000000000000000000", "7b90785125505fad59b13c186dd66ce3" },
    { "ffffffffffffffffffffffffff000000", "00000000000000000000000000000000", "2cb1dc3a9c72972e425ae2ef3eb597cd" }


  };

  for (const auto& test : AES128_ECB_tests)
  {
    crypto::AES128 aes;
    const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
    const std::vector<byte> plain = strings::toByteArray(std::get<1>(test));

    std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
    aes.crypt(plain.data(), cipher.data(), key.data(), plain.size());

    REQUIRE(strings::fromByteArray(cipher) == std::get<2>(test));

    std::vector<byte> original = std::vector<byte>(plain.size(), 0);
    aes.decrypt(cipher.data(), original.data(), key.data(), plain.size());

    REQUIRE(plain == original);
  }
}

TEST_CASE("algorithms.CryptoAES::ecb192")
{
  /* key, plain, cipher */
  const std::vector<std::tuple<std::string, std::string, std::string>> AES192_ECB_tests = {
    { "000000000000000000000000000000000000000000000000", "1b077a6af4b7f98229de786d7516b639", "275cfc0413d8ccb70513c3859b1d0f72" },
    { "000000000000000000000000000000000000000000000000", "9c2d8842e5f48f57648205d39a239af1", "c9b8135ff1b5adc413dfd053b21bd96d" },
    { "000000000000000000000000000000000000000000000000", "bff52510095f518ecca60af4205444bb", "4a3650c3371ce2eb35e389a171427440" },
    { "000000000000000000000000000000000000000000000000", "51719783d3185a535bd75adc65071ce1", "4f354592ff7c8847d2d0870ca9481b7c" },
    { "000000000000000000000000000000000000000000000000", "26aa49dcfe7629a8901a69a9914e6dfd", "d5e08bf9a182e857cf40b3a36ee248cc" },
    { "000000000000000000000000000000000000000000000000", "941a4773058224e1ef66d10e0a6ee782", "067cd9d3749207791841562507fa9626" },

  };

  for (const auto& test : AES192_ECB_tests)
  {
    crypto::AES192 aes;
    const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
    const std::vector<byte> plain = strings::toByteArray(std::get<1>(test));

    std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
    aes.crypt(plain.data(), cipher.data(), key.data(), plain.size());

    REQUIRE(strings::fromByteArray(cipher) == std::get<2>(test));

    std::vector<byte> original = std::vector<byte>(plain.size(), 0);
    aes.decrypt(cipher.data(), original.data(), key.data(), plain.size());

    REQUIRE(plain == original);
  }
}

TEST_CASE("algorithms.CryptoAES::ecb256")
{
  /* key, plain, cipher */
  const std::vector<std::tuple<std::string, std::string, std::string>> AES256_ECB_tests = {
    { "0000000000000000000000000000000000000000000000000000000000000000", "014730f80ac625fe84f026c60bfd547d", "5c9d844ed46f9885085e5d6a4f94c7d7" },
    { "0000000000000000000000000000000000000000000000000000000000000000", "0b24af36193ce4665f2825d7b4749c98", "a9ff75bd7cf6613d3731c77c3b6d0c04" },
    { "0000000000000000000000000000000000000000000000000000000000000000", "761c1fe41a18acf20d241650611d90f1", "623a52fcea5d443e48d9181ab32c7421" },
    { "0000000000000000000000000000000000000000000000000000000000000000", "8a560769d605868ad80d819bdba03771", "38f2c7ae10612415d27ca190d27da8b4" },
    { "0000000000000000000000000000000000000000000000000000000000000000", "91fbef2d15a97816060bee1feaa49afe", "1bc704f1bce135ceb810341b216d7abe" },

  };

  for (const auto& test : AES256_ECB_tests)
  {
    crypto::AES256 aes;
    const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
    const std::vector<byte> plain = strings::toByteArray(std::get<1>(test));

    std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
    aes.crypt(plain.data(), cipher.data(), key.data(), plain.size());

    REQUIRE(strings::fromByteArray(cipher) == std::get<2>(test));

    std::vector<byte> original = std::vector<byte>(plain.size(), 0);
    aes.decrypt(cipher.data(), original.data(), key.data(), plain.size());

    REQUIRE(plain == original);
  }
 }

TEST_CASE("algorithms.CryptoAES::cbc128")
{
  /* key, iv, plain, cipher */
  const std::vector<std::tuple<std::string, std::string, std::string, std::string>> AES128_CBC_tests = {
    { "2b7e151628aed2a6abf7158809cf4f3c", "000102030405060708090A0B0C0D0E0F", "6bc1bee22e409f96e93d7e117393172a", "7649abac8119b246cee98e9b12e9197d" },
    { "2b7e151628aed2a6abf7158809cf4f3c", "7649ABAC8119B246CEE98E9B12E9197D", "ae2d8a571e03ac9c9eb76fac45af8e51", "5086cb9b507219ee95db113a917678b2" },
    { "2b7e151628aed2a6abf7158809cf4f3c", "5086CB9B507219EE95DB113A917678B2", "30c81c46a35ce411e5fbc1191a0a52ef", "73bed6b8e3c1743b7116e69e22229516" }

  };

  for (const auto& test : AES128_CBC_tests)
  {
    crypto::AES128 aes;
    const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
    const std::vector<byte> iv = strings::toByteArray(std::get<1>(test));
    const std::vector<byte> plain = strings::toByteArray(std::get<2>(test));

    std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
    aes.cryptBuffer(plain.data(), cipher.data(), key.data(), plain.size(), iv.data());

    REQUIRE(strings::fromByteArray(cipher) == std::get<3>(test));

    std::vector<byte> original = std::vector<byte>(plain.size(), 0);
    aes.decryptBuffer(cipher.data(), original.data(), key.data(), plain.size(), iv.data());

    REQUIRE(plain == original);
  }
}

TEST_CASE("algorithms.CryptoAES::cbc192")
{
  /* key, iv, plain, cipher */
  const std::vector<std::tuple<std::string, std::string, std::string, std::string>> AES192_CBC_tests = {
    { "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "4F021DB243BC633D7178183A9FA071E8", "ae2d8a571e03ac9c9eb76fac45af8e51", "b4d9ada9ad7dedf4e5e738763f69145a" },
    { "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "B4D9ADA9AD7DEDF4E5E738763F69145A", "30c81c46a35ce411e5fbc1191a0a52ef", "571b242012fb7ae07fa9baac3df102e0" },
    { "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "571B242012FB7AE07FA9BAAC3DF102E0", "f69f2445df4f9b17ad2b417be66c3710", "08b0e27988598881d920a9e64f5615cd" }

  };

  for (const auto& test : AES192_CBC_tests)
  {
    crypto::AES192 aes;
    const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
    const std::vector<byte> iv = strings::toByteArray(std::get<1>(test));
    const std::vector<byte> plain = strings::toByteArray(std::get<2>(test));

    std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
    aes.cryptBuffer(plain.data(), cipher.data(), key.data(), plain.size(), iv.data());

    REQUIRE(strings::fromByteArray(cipher) == std::get<3>(test));

    std::vector<byte> original = std::vector<byte>(plain.size(), 0);
    aes.decryptBuffer(cipher.data(), original.data(), key.data(), plain.size(), iv.data());

    REQUIRE(plain == original);
  }
}

TEST_CASE("algorithms.CryptoAES::cbc256")
{
  /* key, iv, plain, cipher */
  const std::vector<std::tuple<std::string, std::string, std::string, std::string>> AES256_CBC_tests = {
    { "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "000102030405060708090A0B0C0D0E0F", "6bc1bee22e409f96e93d7e117393172a", "f58c4c04d6e5f1ba779eabfb5f7bfbd6" },
    { "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "F58C4C04D6E5F1BA779EABFB5F7BFBD6", "ae2d8a571e03ac9c9eb76fac45af8e51", "9cfc4e967edb808d679f777bc6702c7d" },
    { "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "9CFC4E967EDB808D679F777BC6702C7D", "30c81c46a35ce411e5fbc1191a0a52ef", "39f23369a9d9bacfa530e26304231461" }

  };

  for (const auto& test : AES256_CBC_tests)
  {
    crypto::AES256 aes;
    const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
    const std::vector<byte> iv = strings::toByteArray(std::get<1>(test));
    const std::vector<byte> plain = strings::toByteArray(std::get<2>(test));

    std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
    aes.cryptBuffer(plain.data(), cipher.data(), key.data(), plain.size(), iv.data());

    REQUIRE(strings::fromByteArray(cipher) == std::get<3>(test));

    std::vector<byte> original = std::vector<byte>(plain.size(), 0);
    aes.decryptBuffer(cipher.data(), original.data(), key.data(), plain.size(), iv.data());

    REQUIRE(plain == original);
  }
}
