/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
http://trederia.blogspot.com

crogine application - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#include "SharedStateData.hpp"
#include "CommonConsts.hpp"

std::vector<std::uint8_t> ConnectionData::serialise() const
{
    //connectionID, player count, sizes[5], data

    std::size_t totalSize = 0;
    std::array<std::uint8_t, 5u> sizes = { 0,0,0,0,0 };
    for (auto i = 0u; i < playerCount; ++i)
    {
        sizes[i] = static_cast<std::uint8_t>(std::min(ConstVal::MaxStringDataSize, playerData[i].name.size() * sizeof(std::uint32_t)));

        sizes[i] += sizeof(playerData[i].avatarFlags);
        sizes[i] += sizeof(playerData[i].ballID);
        sizes[i] += sizeof(playerData[i].hairID);
        sizes[i] += sizeof(playerData[i].skinID);
        sizes[i] += sizeof(std::uint8_t); //bool flipped

        totalSize += sizes[i];
    }

    std::vector<std::uint8_t> buffer(totalSize + sizeof(sizes) + 2);
    buffer[0] = connectionID;
    buffer[1] = playerCount;
    buffer[2] = sizes[0];
    buffer[3] = sizes[1];
    buffer[4] = sizes[2];
    buffer[5] = sizes[3];
    buffer[6] = sizes[4];

    std::size_t offset = 2 + sizes.size();
    for (auto i = 0u; i < playerCount && offset < buffer.size(); ++i)
    {
        std::memcpy(&buffer[offset], playerData[i].avatarFlags.data(), sizeof(playerData[i].avatarFlags));
        offset += sizeof(playerData[i].avatarFlags);

        std::memcpy(&buffer[offset], &playerData[i].ballID, sizeof(playerData[i].ballID));
        offset += sizeof(playerData[i].ballID);

        std::memcpy(&buffer[offset], &playerData[i].hairID, sizeof(playerData[i].hairID));
        offset += sizeof(playerData[i].hairID);

        std::memcpy(&buffer[offset], &playerData[i].skinID, sizeof(playerData[i].skinID));
        offset += sizeof(playerData[i].skinID);

        buffer[offset] = playerData[i].flipped ? 1 : 0;
        offset++;



        //do string last as it's not fixed size
        auto stringSize = playerData[i].name.size() * sizeof(std::uint32_t);
        std::memcpy(&buffer[offset], playerData[i].name.data(), stringSize);
        offset += stringSize;
    }

    return buffer;
}

bool ConnectionData::deserialise(const cro::NetEvent::Packet& packet)
{
    //read header
    std::array<std::uint8_t, 5u> sizes = { 0,0,0,0,0 };
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

        std::memcpy(&playerData[i].ballID, ptr + offset, sizeof(playerData[i].ballID));
        offset += sizeof(playerData[i].ballID);
        stringSize -= sizeof(playerData[i].ballID);

        std::memcpy(&playerData[i].hairID, ptr + offset, sizeof(playerData[i].hairID));
        offset += sizeof(playerData[i].hairID);
        stringSize -= sizeof(playerData[i].hairID);

        std::memcpy(&playerData[i].skinID, ptr + offset, sizeof(playerData[i].skinID));
        offset += sizeof(playerData[i].skinID);
        stringSize -= sizeof(playerData[i].skinID);
        
        playerData[i].flipped = ptr[offset] != 0;
        offset++;
        stringSize--;

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