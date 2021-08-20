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

        sizes[i] += sizeof(playerData[i].avatarFlags);
        sizes[i] += sizeof(playerData[i].skinID);

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
        std::memcpy(&buffer[offset], playerData[i].avatarFlags.data(), sizeof(playerData[i].avatarFlags));
        offset += sizeof(playerData[i].avatarFlags);

        std::memcpy(&buffer[offset], &playerData[i].skinID, sizeof(playerData[i].skinID));
        offset += sizeof(playerData[i].skinID);

        //do string last as its not fixed size
        auto stringSize = playerData[i].name.size() * sizeof(std::uint32_t);
        std::memcpy(&buffer[offset], playerData[i].name.data(), stringSize);
        offset += stringSize;
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
        /*if (size % sizeof(std::uint32_t) != 0
            || size > ConstVal::MaxStringDataSize)
        {
            return false;
        }*/
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


    //read player data
    for (auto i = 0u; i < playerCount; ++i)
    {
        auto* ptr = static_cast<const std::uint8_t*>(packet.getData());
        auto stringSize = sizes[i];

        std::memcpy(&playerData[i].avatarFlags, ptr + offset, sizeof(playerData[i].avatarFlags));
        offset += sizeof(playerData[i].avatarFlags);
        stringSize -= sizeof(playerData[i].avatarFlags);

        std::memcpy(&playerData[i].skinID, ptr + offset, sizeof(playerData[i].skinID));
        offset += sizeof(playerData[i].skinID);
        stringSize -= sizeof(playerData[i].skinID);
        


        //check remaining size is valid
        if (stringSize % sizeof(std::uint32_t) != 0
            || stringSize > ConstVal::MaxStringDataSize)
        {
            return false;
        }


        std::vector<std::uint32_t> buffer(stringSize / sizeof(std::uint32_t));
        std::memcpy(buffer.data(), ptr + offset, stringSize);

        playerData[i].name = cro::String::fromUtf32(buffer.begin(), buffer.end());
        offset += stringSize;
    }

    return true;
}