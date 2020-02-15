#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

// ignore assert in Array.hpp for test case
#ifndef NDEBUG
#define NDEBUG
#endif

#include <brynet/base/Array.hpp>

TEST_CASE("Array are computed", "[Array]") {
    using namespace brynet::base;

    const size_t num = 10;
    auto array = array_new(num, sizeof(int));
    REQUIRE(array_num(array) == num);

    int value = 5;
    REQUIRE(array_set(array, 0, &value));
    auto v = array_at(array, 0);
    REQUIRE(v != nullptr);
    REQUIRE(*(int*)v == value);

    REQUIRE(!array_set(array, 10, &value));
    REQUIRE(array_at(array, 10) == nullptr);

    REQUIRE(array_increase(array, num));
    REQUIRE(array_num(array) == (num*2)); 
    v = array_at(array, 0);
    REQUIRE(v != nullptr);
    REQUIRE(*(int*)v == value);

    value++;
    REQUIRE(array_set(array, 19, &value));
    v = array_at(array, 19);
    REQUIRE(v != nullptr);
    REQUIRE(*(int*)v == value);

    REQUIRE(array_at(array, 10) != nullptr);
    REQUIRE(array_at(array, 19) != nullptr);
    REQUIRE(array_at(array, 20) == nullptr);

    array_delete(array);
    array = nullptr;
}