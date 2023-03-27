/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "PlayerData.hpp"
#include "PlayerColours.hpp"
#include "spooky2.hpp"

#include <Social.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/core/ConfigFile.hpp>

bool PlayerData::saveProfile() const
{
    cro::ConfigFile cfg("avatar");

    if (!name.empty())
    {
        cfg.addProperty("name", name.toAnsiString());
    }
    cfg.addProperty("ball_id").setValue(ballID);
    cfg.addProperty("hair_id").setValue(hairID);
    cfg.addProperty("skin_id").setValue(skinID);
    cfg.addProperty("flipped").setValue(flipped);
    cfg.addProperty("flags0").setValue(avatarFlags[0]);
    cfg.addProperty("flags1").setValue(avatarFlags[1]);
    cfg.addProperty("flags2").setValue(avatarFlags[2]);
    cfg.addProperty("flags3").setValue(avatarFlags[3]);
    cfg.addProperty("cpu").setValue(isCPU);

    //hmmmm is it possible we might accidentally
    //create the ID of an existing profile?
    //we're seeding with epoch time to try mitigating this...
    if (profileID.empty())
    {
        std::uint32_t flipVal = flipped ? 0xf0 : 0x0f;
        flipVal |= isCPU ? 0xf000 : 0x0f00;
        std::array<std::uint32_t, 5u> hashData =
        {
            ballID, hairID, skinID, flipVal,
            *reinterpret_cast<const std::uint32_t*>(avatarFlags.data()) //just remember to unbork this when updating palettes later on...
        };
        profileID = std::to_string(SpookyHash::Hash32(hashData.data(), sizeof(std::uint32_t) * hashData.size(), static_cast<std::uint32_t>(cro::SysTime::epoch())));
    }

    auto path = Social::getUserContentPath(Social::UserContent::Profile);
    if (!cro::FileSystem::directoryExists(path))
    {
        cro::FileSystem::createDirectory(path);
    }
    path += profileID + "/";

    if (!cro::FileSystem::directoryExists(path))
    {
        cro::FileSystem::createDirectory(path);
    }

    path += profileID + ".pfl";
    return cfg.save(path);
}

bool PlayerData::loadProfile(const std::string& path, const std::string& uid)
{
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path, false))
    {
        const auto& props = cfg.getProperties();
        for (const auto& prop : props)
        {
            const auto& n = prop.getName();
            if (n == "name")
            {
                name = prop.getValue<cro::String>();
            }
            else if (n == "ball_id")
            {
                auto id = prop.getValue<std::uint32_t>();
                ballID = id;
            }
            else if (n == "hair_id")
            {
                auto id = prop.getValue<std::uint32_t>();
                hairID = id;
            }
            else if (n == "skin_id")
            {
                auto id = prop.getValue<std::uint32_t>();
                skinID = id;
            }
            else if (n == "flipped")
            {
                flipped = prop.getValue<bool>();
            }

            else if (n == "flags0")
            {
                auto flag = prop.getValue<std::int32_t>() % pc::PairCounts[0];
                avatarFlags[0] = static_cast<std::uint8_t>(flag);
            }
            else if (n == "flags1")
            {
                auto flag = prop.getValue<std::int32_t>() % pc::PairCounts[1];
                avatarFlags[1] = static_cast<std::uint8_t>(flag);
            }
            else if (n == "flags2")
            {
                auto flag = prop.getValue<std::int32_t>() % pc::PairCounts[2];
                avatarFlags[2] = static_cast<std::uint8_t>(flag);
            }
            else if (n == "flags3")
            {
                auto flag = prop.getValue<std::int32_t>() % pc::PairCounts[3];
                avatarFlags[3] = static_cast<std::uint8_t>(flag);
            }

            else if (n == "cpu")
            {
                isCPU = prop.getValue<bool>();
            }
        }

        profileID = uid;

        mugshot = cro::FileSystem::getFilePath(path) + "mug.png";
        if (!cro::FileSystem::fileExists(mugshot))
        {
            mugshot.clear();
        }

        return true;
    }
    return false;
}

//------------------------------------------------------------

ProfileTexture::ProfileTexture(const std::string& path)
    : m_imageBuffer (std::make_unique<cro::Image>()),
    m_texture       (std::make_unique<cro::Texture>())
{
    if (m_imageBuffer->loadFromFile(path) //MUST be RGBA for colour replace to work.
        && m_imageBuffer->getFormat() == cro::ImageFormat::RGBA)
    {
        //cache color indices
        auto stride = 4u;
        auto length = m_imageBuffer->getSize().x * m_imageBuffer->getSize().y * stride;
        const auto* pixels = m_imageBuffer->getPixelData();

        for (auto i = 0u; i < length; i += stride)
        {
            auto alpha = pixels[i + (stride - 1)];

            switch (pixels[i])
            {
            default: break;
            case pc::Keys[pc::ColourKey::Bottom].light:
                if (alpha) m_keyIndicesLight[pc::ColourKey::Bottom].push_back(i / stride);
                break;
            case pc::Keys[pc::ColourKey::Bottom].dark:
                if (alpha) m_keyIndicesDark[pc::ColourKey::Bottom].push_back(i / stride);
                break;

            case pc::Keys[pc::ColourKey::Top].light:
                if (alpha) m_keyIndicesLight[pc::ColourKey::Top].push_back(i / stride);
                break;
            case pc::Keys[pc::ColourKey::Top].dark:
                if (alpha) m_keyIndicesDark[pc::ColourKey::Top].push_back(i / stride);
                break;

            case pc::Keys[pc::ColourKey::Skin].light:
                if (alpha) m_keyIndicesLight[pc::ColourKey::Skin].push_back(i / stride);
                break;
            case pc::Keys[pc::ColourKey::Skin].dark:
                if (alpha) m_keyIndicesDark[pc::ColourKey::Skin].push_back(i / stride);
                break;

            case pc::Keys[pc::ColourKey::Hair].light:
                if (alpha) m_keyIndicesLight[pc::ColourKey::Hair].push_back(i / stride);
                break;
            case pc::Keys[pc::ColourKey::Hair].dark:
                if (alpha) m_keyIndicesDark[pc::ColourKey::Hair].push_back(i / stride);
                break;
            }
        }

        apply();
    }
    else
    {
        LogE << path << ": file not loaded or not RGBA" << std::endl;
        m_imageBuffer->create(48, 52, cro::Colour::Magenta);
    }
}

void ProfileTexture::setColour(pc::ColourKey::Index idx, std::int8_t pairIdx)
{
    CRO_ASSERT(pairIdx < pc::ColourID::Count, "");

    const auto imgWidth = m_imageBuffer->getSize().x;
    //cache the colour and only update if it changed.
    if (auto colour = cro::Colour(pc::Palette[pairIdx].light); m_lightColours[idx] != colour)
    {

        for (auto i : m_keyIndicesLight[idx])
        {
            //set the colour in the image at this index
            auto x = i % imgWidth;
            auto y = i / imgWidth;

            m_imageBuffer->setPixel(x, y, colour);
        }
        m_lightColours[idx] = colour;
    }


    if (auto colour = cro::Colour(pc::Palette[pairIdx].dark); m_darkColours[idx] != colour)
    {
        for (auto i : m_keyIndicesDark[idx])
        {
            auto x = i % imgWidth;
            auto y = i / imgWidth;

            m_imageBuffer->setPixel(x, y, colour);
        }
        m_darkColours[idx] = colour;
    }
}

std::pair<cro::Colour, cro::Colour> ProfileTexture::getColour(pc::ColourKey::Index idx) const
{
    return { m_lightColours[idx], m_darkColours[idx] };
}

void ProfileTexture::apply(cro::Texture* dst)
{
    if (dst)
    {
        dst->loadFromImage(*m_imageBuffer);
    }
    else
    {
        m_texture->loadFromImage(*m_imageBuffer);
    }
}

void ProfileTexture::setMugshot(const std::string& path)
{
    if (cro::FileSystem::fileExists(path))
    {
        m_mugshot = std::make_unique<cro::Texture>();
        m_mugshot->loadFromFile(path);
    }
}

const cro::Texture* ProfileTexture::getMugshot() const
{
    return m_mugshot.get();
}