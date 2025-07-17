/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2023
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

#include "Achievements.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>

#include <vector>
#include <unordered_map>
#include <memory>
#include <array>

namespace cro
{
    class MessageBus;
}

//default implementation
class DefaultAchievements final : public AchievementImpl
{
public:
    DefaultAchievements();

    bool init() override;
    void update() override;
    void awardAchievement(const std::string&) override;
    const AchievementData* getAchievement(const std::string&) const override;
    AchievementImage getIcon(const std::string&) const override;
    void setStat(const std::string&, std::int32_t) override;
    void setStat(const std::string&, float) override;
    float incrementStat(const std::string&, std::int32_t) override;
    float incrementStat(const std::string&, float) override;
    const StatData* getStat(const std::string&) const override;

    void setAvgStat(const std::string&, float v, float d) const override {};
    float getAvgStat(const std::string&) const override { return 0.f; };

#ifdef CRO_DEBUG_
    void showTest();
#endif

    void drawOverlay();

    void clearAchievement(const std::string&);

private:
    cro::MessageBus& m_messageBus;

    std::unordered_map<std::string, AchievementData> m_achievements;
    std::unordered_map<std::string, StatData> m_stats;

    cro::Font m_font;
    cro::Texture m_texture;

    void syncStat(const StatData&);

    //8 * 32 achievements = 256, should be plenty :)
    std::array<std::uint32_t, 8> m_bitArray = {};
    std::array<float, 64> m_statArray = {};
    std::array<std::uint64_t, 256> m_timeStamps = {};
    void readFile();
    void writeFile();
    bool readBit(std::int32_t);
    void writeBit(std::int32_t);
    void clearBit(std::int32_t);

    cro::Clock m_updateClock;
    bool m_statsUpdated = false;

    class AchievementIcon final : public cro::Transformable2D
    {
    public:
        AchievementIcon(const AchievementData&, DefaultAchievements&);

        void update(float);
        bool complete() const { return m_complete; }

        void draw(const glm::mat4&);

    private:
        bool m_complete;
        cro::SimpleVertexArray m_background;
        cro::SimpleQuad m_sprite;
        cro::SimpleText m_text;
        cro::SimpleText m_titleText;

        enum
        {
            ScrollIn, Paused, ScrollOut
        }m_state;
        float m_pauseTime;
    };
    friend class AchievementIcon;

    std::vector<std::unique_ptr<AchievementIcon>> m_icons;
};
