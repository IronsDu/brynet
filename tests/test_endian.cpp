#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include <brynet/base/endian/Endian.hpp>

TEST_CASE("Endian are computed", "[Endian]") {
    using namespace brynet::base::endian;

    auto littleEndian = false;
    {
        int a = 0x12345678;
        littleEndian = (*(char*)&a == 0x78);
    }

    REQUIRE(hostToNetwork64(0x1234567812345678, littleEndian) == 0x7856341278563412);
    REQUIRE(hostToNetwork32(0x12345678, littleEndian) == 0x78563412);
    REQUIRE(hostToNetwork16(0x1234, littleEndian) == 0x3412);

    REQUIRE(networkToHost64(0x1234567812345678, littleEndian) == 0x7856341278563412);
    REQUIRE(networkToHost32(0x12345678, littleEndian) == 0x78563412);
    REQUIRE(networkToHost16(0x1234, littleEndian) == 0x3412);
}
