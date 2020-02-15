#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include <brynet/base/Buffer.hpp>

TEST_CASE("Buffer are computed", "[Buffer]") {
    using namespace brynet::base;

    auto buffer = buffer_new(100);
    REQUIRE(buffer_getreadvalidcount(buffer) == 0);
    REQUIRE(buffer_getwritepos(buffer) == 0);
    REQUIRE(buffer_getreadpos(buffer) == 0);
    REQUIRE(buffer_getwritevalidcount(buffer) == 100);
    REQUIRE(buffer_getsize(buffer) == 100);

    int value = 5;
    REQUIRE(buffer_write(buffer, (const char*)&value, sizeof(value)));
    REQUIRE(buffer_getreadvalidcount(buffer) == sizeof(value));
    REQUIRE(buffer_getwritepos(buffer) == sizeof(value));
    REQUIRE(buffer_getreadpos(buffer) == 0);
    REQUIRE(buffer_getwritevalidcount(buffer) == (100- sizeof(value)));
    REQUIRE(buffer_getsize(buffer) == 100);

    REQUIRE(*(int*)buffer_getreadptr(buffer) == 5);
    REQUIRE(buffer_getreadptr(buffer) + sizeof(value) == buffer_getwriteptr(buffer));

    buffer_adjustto_head(buffer);
    REQUIRE(*(int*)buffer_getreadptr(buffer) == 5);
    REQUIRE(buffer_getreadptr(buffer) + sizeof(value) == buffer_getwriteptr(buffer));

    char bigBuffer[200];
    REQUIRE(!buffer_write(buffer, bigBuffer, sizeof(bigBuffer)));

    buffer_delete(buffer);
    buffer = nullptr;
}