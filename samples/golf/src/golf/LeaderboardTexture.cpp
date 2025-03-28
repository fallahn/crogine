/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "LeaderboardTexture.hpp"
#include "MenuConsts.hpp"

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/detail/Assert.hpp>

LeaderboardTexture::LeaderboardTexture()
{

}

//public
void LeaderboardTexture::init(const cro::Sprite& backgroundSprite, cro::Font& font)
{
    auto textureRect = backgroundSprite.getTextureRect();
    m_backgroundSprite.setTexture(*backgroundSprite.getTexture());
    m_backgroundSprite.setTextureRect(textureRect);

    m_text.setFont(font);
    m_text.setCharacterSize(UITextSize);
    m_text.setFillColour(LeaderboardTextDark);
    m_text.setVerticalSpacing(LeaderboardTextSpacing);
    m_text.setCroppingArea({ -MinLobbyCropWidth, 20.f, MinLobbyCropWidth * 2.f, -((textureRect.height * 2.f) + 20.f) });

    m_texture.create(static_cast<std::uint32_t>(textureRect.width),
                    static_cast<std::uint32_t>(textureRect.height) * 2, false);

    for (auto target : m_targetEnts)
    {
        target.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", cro::TextureID(m_texture.getTexture().getGLHandle()));
    }
}

void LeaderboardTexture::update(std::vector<LeaderboardEntry>& entries)
{
    //this is the offset between the background border and the background
    //sprite - really we should be loading this from sprite data else it won't
    //account for any future changes in size...
    static constexpr glm::vec2 Border(6.f, 20.f);

    //both pages exist in a single string
    //so we offset the strings before drawing the second page
    auto offset = static_cast<float>(m_texture.getSize().y / 2);
    CRO_ASSERT(offset != 0, "");

    m_texture.clear(cro::Colour::Red);
    m_backgroundSprite.setPosition({ 0.f, 0.f });
    m_backgroundSprite.draw();
    m_backgroundSprite.setPosition({ 0.f, offset });
    m_backgroundSprite.draw();

    std::vector<std::int32_t> alignments(entries.size());
    std::fill(alignments.begin(), alignments.end(), cro::SimpleText::Alignment::Right);
    alignments[0] = cro::SimpleText::Alignment::Left;
    alignments.back() = cro::SimpleText::Alignment::Left;
    
    std::int32_t i = 0;
    for (auto& [position, str] : entries)
    {
        position -= Border;
        position.y += offset;

        position.x = std::floor(position.x);
        position.y = std::floor(position.y);

        m_text.setPosition(position);
        m_text.setAlignment(alignments[i++]);
        m_text.setString(str);
        m_text.draw();
    }

    m_texture.display();
}

void LeaderboardTexture::addTarget(cro::Entity e)
{
    m_targetEnts.push_back(e);
}

const cro::Texture& LeaderboardTexture::getTexture() const
{
    return m_texture.getTexture();
}