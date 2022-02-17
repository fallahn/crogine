/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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

#pragma once

#include "Achievements.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/Texture.hpp>

#include <vector>
#include <unordered_map>
#include <memory>
#include <array>

//default implementation for testing
class DefaultAchievements final : public AchievementImpl
{
public:
    void init() override;
    void update() override;
    void registerAchievement(const std::string&) override;
    void awardAchievement(const std::string&) override;
    const AchievementData* getAchievement(const std::string&) const override;
    void setStat(const std::string&, std::int32_t) override;
    float incrementStat(const std::string&, std::int32_t) override;

    void setWindowSize(glm::vec2);

#ifdef CRO_DEBUG_
    void showTest();
#endif


private:
    std::unordered_map<std::string, AchievementData> m_achievements;
    std::unordered_map<std::string, StatData> m_stats;

    cro::Font m_font;
    cro::Texture m_texture;

    void syncStat(const StatData&);

    //8 * 32 achievements = 256, should be plenty :)
    std::array<std::uint32_t, 8> m_bitArray = {};
    std::array<std::int32_t, 64> m_statArray = {};
    void readFile();
    void writeFile();
    bool readBit(std::int32_t);
    void writeBit(std::int32_t);

    cro::Clock m_updateClock;
    bool m_statsUpdated = false;

    /*class AchievementIcon final : public sf::Drawable, public sf::Transformable
    {
    public:
        AchievementIcon(const AchievementData&, DefaultAchievements&);

        void update(float);
        bool complete() const { return m_complete; }

    private:
        bool m_complete;
        sf::RectangleShape m_background;
        std::array<sf::Vertex, 4u> m_gradient = {};
        sf::Sprite m_sprite;
        sf::Text m_text;
        sf::Text m_titleText;

        enum
        {
            ScrollIn, Paused, ScrollOut
        }m_state;
        float m_pauseTime;

        void draw(sf::RenderTarget&, sf::RenderStates) const override;
    };
    friend class AchievementIcon;

    std::vector<std::unique_ptr<AchievementIcon>> m_icons;
    void draw(sf::RenderTarget&, sf::RenderStates) const override;*/
};
