/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/ecs/Entity.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>

namespace cro
{
    class Sprite;
}

struct LeaderboardEntry final
{
    glm::vec2 position = glm::vec2(0.f);
    cro::String str;
    LeaderboardEntry(glm::vec2 p, const cro::String& s)
        : position(p), str(s) {}
};

//dynamic textures to apply the current leaderboard to in-game model
class LeaderboardTexture final
{
public:
    LeaderboardTexture();

    void init(const cro::Sprite&, class cro::Font&);
    void update(std::vector<LeaderboardEntry>&);

    const cro::Texture& getTexture() const;
    
    //hack to get around the fact that these ents
    //are created before the leaderboard is initialised
    void addTarget(cro::Entity);

private:
    cro::SimpleQuad m_backgroundSprite;
    cro::SimpleText m_text;

    cro::RenderTexture m_texture;

    std::vector<cro::Entity> m_targetEnts;
};
