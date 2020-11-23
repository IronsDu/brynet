#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include <brynet/base/Packet.hpp>

TEST_CASE("Packet are computed", "[Packet]") {
    using namespace brynet::base;
    BigPacket packet;
    packet.writeBool(true);
    packet.writeINT8(1);
    packet.writeUINT8(2);
    packet.writeINT16(3);
    packet.writeUINT16(4);
    packet.writeINT32(5);
    packet.writeUINT32(6);
    packet.writeINT64(7);
    packet.writeUINT64(8);

    REQUIRE(packet.getPos() == 31);

    BasePacketReader reader(packet.getData(), packet.getPos());
    REQUIRE(reader.getLeft() == 31);
    REQUIRE(reader.readBool() == true);
    REQUIRE(reader.readINT8() == 1);
    REQUIRE(reader.readUINT8() == 2);
    REQUIRE(reader.readINT16() == 3);
    REQUIRE(reader.readUINT16() == 4);
    REQUIRE(reader.readINT32() == 5);
    REQUIRE(reader.readUINT32() == 6);
    REQUIRE(reader.readINT64() == 7);
    REQUIRE(reader.readUINT64() == 8);

    REQUIRE(reader.getLeft() == 0);
    REQUIRE(reader.currentPos() == 31);
}