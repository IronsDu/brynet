#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include <brynet/base/Stack.hpp>

TEST_CASE("Stack are computed", "[Stack]") {
    using namespace brynet::base;

    const size_t StackNum = 10;

    auto stack = stack_new(StackNum, sizeof(size_t));
    REQUIRE(stack_num(stack) == 0);
    REQUIRE(stack_size(stack) == StackNum);
    REQUIRE(!stack_isfull(stack));
    REQUIRE(stack_popback(stack) == nullptr);
    REQUIRE(stack_popfront(stack) == nullptr);

    for (size_t i = 0; i < StackNum; i++)
    {
        REQUIRE(stack_push(stack, &i));
    }
    REQUIRE(stack_num(stack) == StackNum);
    REQUIRE(stack_isfull(stack));

    for (size_t i = 0; i < StackNum; i++)
    {
        REQUIRE(*(size_t*)stack_popfront(stack) == i);
    }
    REQUIRE(stack_popback(stack) == nullptr);
    REQUIRE(stack_popfront(stack) == nullptr);

    stack_init(stack);

    REQUIRE(!stack_isfull(stack));
    REQUIRE(stack_popback(stack) == nullptr);
    REQUIRE(stack_popfront(stack) == nullptr);
    for (size_t i = 0; i < StackNum; i++)
    {
        REQUIRE(stack_push(stack, &i));
    }
    for (int i = StackNum-1; i >= 0; i--)
    {
        REQUIRE(*(size_t*)stack_popback(stack) == (size_t)i);
    }
    REQUIRE(stack_popback(stack) == nullptr);
    REQUIRE(stack_popfront(stack) == nullptr);

    stack_delete(stack);
    stack = nullptr;
}