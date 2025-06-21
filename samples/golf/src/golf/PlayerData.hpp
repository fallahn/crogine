/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2025
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#pragma once

#include "PlayerColours.hpp"
#include "Inventory.hpp"

#include <crogine/core/String.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/ImageArray.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <crogine/util/Random.hpp>

#include <array>
#include <memory>

namespace sv
{
    struct PlayerInfo;
}

struct PlayerData final
{
    /*
    SIGH this is partially duplicated in sv::PlayerInfo (the serialised
    parts at least) so REMEMBER if we add a new field here it probably
    needs duplicating in sv::PlayerInfo too. We'll use the assignment
    overload to try and contain this somewhat (though both structs will
    still need updating) - See ServerState.hpp
    */
    PlayerData& operator = (const sv::PlayerInfo&);

    cro::String name;
    std::array<std::uint8_t, pc::ColourKey::Count> avatarFlags = 
    { 
        std::uint8_t(cro::Util::Random::value(0u, pc::PairCounts[0] - 1)),
        std::uint8_t(cro::Util::Random::value(0u, pc::PairCounts[1] - 1)),
        std::uint8_t(cro::Util::Random::value(0u, pc::PairCounts[2] - 1)),
        std::uint8_t(cro::Util::Random::value(0u, pc::PairCounts[3] - 1)),
        std::uint8_t(cro::Util::Random::value(0u, pc::PairCounts[4] - 1)),
        std::uint8_t(cro::Util::Random::value(0u, pc::PairCounts[5] - 1)),   
        std::uint8_t(cro::Util::Random::value(0u, pc::PairCounts[6] - 1)),   
    }; //indices into colours
    std::uint8_t ballColourIndex = 255; //ignore the palette if not in range and set to white
    std::uint32_t ballID = 0;
    std::uint32_t clubID = 0;
    std::uint32_t hairID = 0;
    std::uint32_t hatID = 0;
    std::uint32_t skinID = 0; //uid as loaded from the avatar data file
    std::uint32_t voiceID = 0;
    std::int8_t voicePitch = 0;
    bool flipped = false; //whether or not avatar flipped/southpaw
    bool isCPU = false; //these bools are flagged as bits in a single byte when serialised

    struct HeadwearOffset final
    {
        enum
        {
            HairTx, HairRot, HairScale,
            HatTx, HatRot, HatScale,
            Count
        };
    };
    std::array<glm::vec3, HeadwearOffset::Count> headwearOffsets = {};

    //these aren't included in serialise/deserialise
    std::vector<std::uint8_t> holeScores;
    std::vector<bool> holeComplete; //TODO surely we don't need to record this for *every* hole?
    std::vector<float> distanceScores;
    bool targetHit = false; //used in multi-target mode to decide what to look at
    std::uint8_t score = 0;
    std::uint8_t matchScore = 0;
    std::uint8_t skinScore = 0;
    std::int32_t parScore = 0;
    glm::vec3 currentTarget = glm::vec3(0.f);
    cro::Colour ballTint;
    cro::Colour ballColour = cro::Colour::White;

    //this should probably come from the server to ensure sync
    std::int32_t teamIndex = -1;
    bool activeTeamMember = false; //set this on player change so we know if we should draw the ball

    //this is client side profile specific data
    cro::ImageArray<std::uint8_t> mugshotData; //pixel data of the mugshot for avatar icon
    std::string mugshot; //path to mugshot if it exists
    mutable std::string profileID; //saving file generates this

    bool saveProfile() const;
    bool loadProfile(const std::string& path, const std::string& uid);

    bool isSteamID = false;
    mutable bool isCustomName = false; //if not true and is a steam profile use the current steam name

    PlayerData()
    {
        std::fill(headwearOffsets.begin(), headwearOffsets.end(), glm::vec3(0.f));
        headwearOffsets[HeadwearOffset::HairScale] = glm::vec3(1.f);
        headwearOffsets[HeadwearOffset::HatScale] = glm::vec3(1.f);
    }
};

struct ProfileTexture
{
    explicit ProfileTexture(const std::string&);

    void setColour(pc::ColourKey::Index, std::int8_t);
    const cro::Colour& getColour(pc::ColourKey::Index) const;
    void apply(cro::Texture* dst = nullptr);

    cro::TextureID getTextureID() const { return cro::TextureID(*m_texture); }
    cro::Texture* getTexture() const { return m_texture.get(); }

    void setMugshot(const std::string&);
    const cro::Texture* getMugshot() const;

private:
    //make these pointers because this struct is
    //stored in a vector
    std::unique_ptr<cro::Image> m_imageBuffer;
    std::unique_ptr<cro::Texture> m_texture;
    std::unique_ptr<cro::Texture> m_mugshot;

    std::array<std::vector<std::uint32_t>, pc::ColourKey::Count> m_keyIndices;

    std::array<cro::Colour, pc::ColourKey::Count> m_colours;
};