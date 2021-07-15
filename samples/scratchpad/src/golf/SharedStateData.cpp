#include "SharedStateData.hpp"
#include "CommonConsts.hpp"

std::vector<std::uint8_t> ConnectionData::serialise() const
{
    //connectionID, player count, sizes[4], data

    std::size_t totalSize = 0;
    std::array<std::uint8_t, 4u> sizes = { 0,0,0,0 };
    for (auto i = 0u; i < playerCount; ++i)
    {
        sizes[i] = static_cast<std::uint8_t>(std::min(ConstVal::MaxStringDataSize, playerData[i].name.size() * sizeof(std::uint32_t)));
        totalSize += sizes[i];
    }

    std::vector<std::uint8_t> buffer(totalSize + sizeof(sizes) + 2);
    buffer[0] = connectionID;
    buffer[1] = playerCount;
    buffer[2] = sizes[0];
    buffer[3] = sizes[1];
    buffer[4] = sizes[2];
    buffer[5] = sizes[3];

    std::size_t offset = 6;
    for (auto i = 0u; i < sizes.size() && offset < buffer.size(); ++i)
    {
        std::memcpy(&buffer[offset], playerData[i].name.data(), sizes[i]);
        offset += sizes[i];
    }

    return buffer;
}

bool ConnectionData::deserialise(const cro::NetEvent::Packet& packet)
{
    //read header
    std::array<std::uint8_t, 4u> sizes = { 0,0,0,0 };
    if (packet.getSize() < sizeof(sizes) + 2)
    {
        return false;
    }

    std::memcpy(&connectionID, packet.getData(), 1);
    std::memcpy(&playerCount, static_cast<const std::uint8_t*>(packet.getData()) + 1, 1);

    if (playerCount == 0
        || playerCount > MaxPlayers)
    {
        return false;
    }

    std::size_t offset = 2;
    std::memcpy(sizes.data(), static_cast<const std::uint8_t*>(packet.getData()) + offset, sizeof(sizes));

    //check sizes are valid
    std::size_t totalSize = 0;
    for (auto size : sizes)
    {
        if (size % sizeof(std::uint32_t) != 0
            || size > ConstVal::MaxStringDataSize)
        {
            return false;
        }
        totalSize += size;
    }

    if (totalSize == 0)
    {
        //strings were empty
        return false;
    }

    offset += sizeof(sizes);

    if (totalSize + offset != packet.getSize())
    {
        //something didn't add up
        return false;
    }


    //read strings
    for (auto i = 0u; i < playerCount; ++i)
    {
        std::vector<std::uint32_t> buffer(sizes[i] / sizeof(std::uint32_t));
        std::memcpy(buffer.data(), static_cast<const std::uint8_t*>(packet.getData()) + offset, sizes[i]);

        playerData[i].name = cro::String::fromUtf32(buffer.begin(), buffer.end());
        offset += sizes[i];
    }

    return true;
}