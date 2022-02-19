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

#include "DefaultAchievements.hpp"
#include "AchievementStrings.hpp"

#include <crogine/core/App.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/util/Random.hpp>

#include <stdio.h>
#include <sys/stat.h>

namespace
{
    const std::string FileName = "progress.stats";
    const cro::Time UpdateTime = cro::seconds(5.f);

    constexpr glm::vec2 IconSize(242.f, 92.f);
}

void DefaultAchievements::init()
{
    LOG("Initialised default Achievements system", cro::Logger::Type::Info);

    readFile();

    m_texture.loadFromFile(cro::FileSystem::getResourcePath() + "assets/images/achievements.png");
    m_texture.setRepeated(true);
    m_font.loadFromFile(cro::FileSystem::getResourcePath() + "assets/golf/fonts/ProggyClean.ttf");

    for (auto i = 1; i < AchievementID::Count; ++i)
    {
        m_achievements.insert(std::make_pair(AchievementStrings[i], AchievementData(AchievementLabels[i], i, readBit(i))));
    }

    for (auto i = 0; i < StatID::Count; ++i)
    {
        m_stats.insert(std::make_pair(StatStrings[i], StatData(StatStrings[i], i, m_statArray[i])));
    }

    //set achievements to be triggered by stats
    auto* trigger = &StatTriggers[StatID::HolesPlayed].emplace_back();
    trigger->achID = AchievementID::BullsEye;
    trigger->threshold = 10;

    trigger = &StatTriggers[StatID::PuttDistance].emplace_back();
    trigger->achID = AchievementID::LongDistanceClara;
    trigger->threshold = 1000; //metres

    trigger = &StatTriggers[StatID::StrokeDistance].emplace_back();
    trigger->achID = AchievementID::StrokeOfMidnight;
    trigger->threshold = 12000; //metres
}

void DefaultAchievements::update()
{
    const float dt = 1.f / 60.f; //haaaaxaxxxxx

    for (auto& icon : m_icons)
    {
        icon->update(dt);
    }

    m_icons.erase(std::remove_if(m_icons.begin(), m_icons.end(),
        [](const std::unique_ptr<AchievementIcon>& icon)
    {
        return icon->complete();
    }), m_icons.end());


    //write this periodically to prevent fast stat updates
    //hammering the filesystem.
    if (m_statsUpdated
        && m_updateClock.elapsed() > UpdateTime)
    {
        m_updateClock.restart();
        m_statsUpdated = false;

        writeFile();
    }
}

void DefaultAchievements::registerAchievement(const std::string&)
{

}

void DefaultAchievements::awardAchievement(const std::string& name)
{
    if (m_achievements.count(name) != 0 && !m_achievements[name].achieved)
    {
        LOG("Awarded achievement " + name, cro::Logger::Type::Info);
        m_icons.emplace_back(std::make_unique<AchievementIcon>(m_achievements[name], *this));
        m_achievements[name].achieved = true;

        writeBit(m_achievements[name].id);
        writeFile();
    }
}

const AchievementData* DefaultAchievements::getAchievement(const std::string& name) const
{
    if (m_achievements.count(name) != 0)
    {
        return &m_achievements.at(name);
    }
    return nullptr;
}

void DefaultAchievements::setStat(const std::string& name, std::int32_t value)
{
    if (m_stats.count(name) != 0)
    {
        CRO_ASSERT(m_stats[name].id > -1, "");

        LOG("Set stat " + name + " to " + std::to_string(value), cro::Logger::Type::Info);

        auto& stat = m_stats[name];
        stat.value = static_cast<float>(value);
        syncStat(stat);
    }
}

float DefaultAchievements::incrementStat(const std::string& name, std::int32_t value)
{
    if (m_stats.count(name) != 0)
    {
        CRO_ASSERT(m_stats[name].id > -1, "");

        LOG("Incremented stat " + name + " by " + std::to_string(value), cro::Logger::Type::Info);

        auto& stat = m_stats[name];
        stat.value += static_cast<float>(value);
        syncStat(stat);
    }
    return 0.f;
}

float DefaultAchievements::incrementStat(const std::string& name, float value)
{
    if (m_stats.count(name) != 0)
    {
        CRO_ASSERT(m_stats[name].id > -1, "");

        LOG("Incremented stat " + name + " by " + std::to_string(value), cro::Logger::Type::Info);

        auto& stat = m_stats[name];
        stat.value += value;
        syncStat(stat);
    }
    return 0.f;
}

#ifdef CRO_DEBUG_
void DefaultAchievements::showTest()
{
    /*static std::size_t idx = 1;
    m_icons.emplace_back(std::make_unique<AchievementIcon>(m_achievements[AchievementStrings[idx]], *this));
    idx = (idx + 1) % AchievementID::Count;*/

    //m_icons.emplace_back(std::make_unique<AchievementIcon>(m_achievements[AchievementStrings[1]], *this));
    //m_icons.emplace_back(std::make_unique<AchievementIcon>(m_achievements[AchievementStrings[2]], *this));
    m_icons.emplace_back(std::make_unique<AchievementIcon>(m_achievements[AchievementStrings[cro::Util::Random::value(1, AchievementID::Count - 1)]], *this));
}
#endif

void DefaultAchievements::drawOverlay()
{
    glm::vec2 windowSize = cro::App::getWindow().getSize();

    glm::mat4 tx = glm::translate(glm::mat4(1.f), glm::vec3(windowSize.x - IconSize.x, 0.f, 0.f));
    std::vector<glm::mat4> transforms(m_icons.size());

    std::size_t j = transforms.size() - 1;
    for (auto i = m_icons.crbegin(); i != m_icons.crend(); ++i, --j)
    {
        transforms[j] = tx;
        tx = glm::translate(tx, glm::vec3(0.f, i->get()->getPosition().y + IconSize.y, 0.f));
    }

    //unfortunately we need to draw in the opposite order
    for (auto i = 0u; i < transforms.size(); ++i)
    {
        m_icons[i]->draw(transforms[i]);
    }
}

//private
void DefaultAchievements::syncStat(const StatData& stat)
{
    m_statArray[stat.id] = stat.value;

    m_statsUpdated = true;

    //check if this should trigger an achievement
    auto& triggers = StatTriggers[stat.id];

    for (const auto& t : triggers)
    {
        if (stat.value >= t.threshold)
        {
            awardAchievement(AchievementStrings[t.achID]);
        }
    }

    //and remove completed
    triggers.erase(std::remove_if(triggers.begin(), triggers.end(),
        [&stat](const StatTrigger& t)
        {
            return stat.value >= t.threshold;
        }), triggers.end());
}

void DefaultAchievements::readFile()
{
    std::fill(m_bitArray.begin(), m_bitArray.end(), 0);
    std::fill(m_statArray.begin(), m_statArray.end(), 0.f);

    std::size_t bitsize = sizeof(std::uint32_t) * m_bitArray.size();
    std::size_t statsize = sizeof(std::int32_t) * m_statArray.size();
    const std::string filePath = cro::App::getInstance().getPreferencePath() + FileName;

    if (cro::FileSystem::fileExists(filePath))
    {
        struct stat st;
        stat(filePath.c_str(), &st);

        FILE* inFile = fopen(filePath.c_str(), "rb");
        if (inFile && st.st_size == (bitsize + statsize))
        {
            auto read = fread(m_bitArray.data(), bitsize, 1, inFile);
            if (read != 1)
            {
                LogW << "Failed reading achievement status" << std::endl;
            }

            read = fread(m_statArray.data(), statsize, 1, inFile);
            if (read != 1)
            {
                LogW << "Failed reading stats" << std::endl;
            }

            fclose(inFile);
        }
        else
        {
            writeFile();
        }
    }
}

void DefaultAchievements::writeFile()
{
    std::size_t bitsize = sizeof(std::uint32_t) * m_bitArray.size();
    std::size_t statsize = sizeof(std::int32_t) * m_statArray.size();
    const std::string filePath = cro::App::getInstance().getPreferencePath() + FileName;

    FILE* outFile = fopen(filePath.c_str(), "wb");
    if (outFile)
    {
        fwrite(m_bitArray.data(), bitsize, 1, outFile);
        fwrite(m_statArray.data(), statsize, 1, outFile);
        fclose(outFile);
    }
    else
    {
        LogE << "failed to write achievement data" << std::endl;
    }
}

bool DefaultAchievements::readBit(std::int32_t index)
{
    CRO_ASSERT(index > -1, "");

    auto offset = index / 32;
    auto bit = index % 32;

    CRO_ASSERT(offset < m_bitArray.size(), "");
    return (m_bitArray[offset] & (1 << bit)) != 0;
}

void DefaultAchievements::writeBit(std::int32_t index)
{
    CRO_ASSERT(index > -1, "");

    auto offset = index / 32;
    auto bit = index % 32;

    CRO_ASSERT(offset < m_bitArray.size(), "");
    m_bitArray[offset] |= (1 << bit);
}

//----------------------------------

DefaultAchievements::AchievementIcon::AchievementIcon(const AchievementData& data, DefaultAchievements& da)
    : m_complete    (false),
    m_state         (ScrollIn),
    m_pauseTime     (5.f)
{
    //assemble parts
    const cro::Colour BackgroundColour(std::uint8_t(37), 36, 34);
    m_background.setVertexData(
    {
        //border
        cro::Vertex2D(glm::vec2(0.f, IconSize.y), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::Black),
        cro::Vertex2D(IconSize, cro::Colour::Black),
        cro::Vertex2D(glm::vec2(IconSize.x, 0.f), cro::Colour::Black),

        //background
        cro::Vertex2D(glm::vec2(1.f, IconSize.y - 1.f), BackgroundColour),
        cro::Vertex2D(glm::vec2(1.f), BackgroundColour),
        cro::Vertex2D(IconSize - glm::vec2(1.f), BackgroundColour),
        cro::Vertex2D(glm::vec2(IconSize.x, 1.f), BackgroundColour),

        //gradient
        cro::Vertex2D(glm::vec2(2.f, IconSize.y - 2.f), cro::Colour::Transparent),
        cro::Vertex2D(glm::vec2(2.f, 2.f), cro::Colour()),
        cro::Vertex2D(glm::vec2(IconSize.x - 2.f, IconSize.y - 2.f), cro::Colour::Transparent),
        cro::Vertex2D(glm::vec2(IconSize.x - 2.f, 2.f), cro::Colour())
    });

    m_text.setFont(da.m_font);
    m_text.setCharacterSize(16);
    m_text.setString(data.name);
    m_text.setPosition({ 86.f, 34.f });

    m_titleText.setFont(da.m_font);
    m_titleText.setCharacterSize(16);
    m_titleText.setString("ACHIEVEMENT UNLOCKED!");
    m_titleText.setPosition({ 86.f, 54.f });

    const std::int32_t width = 5; //number of icons per row
    auto x = data.id % width;
    auto y = data.id / width;

    m_sprite.setPosition({ 12.f, 16.f });
    m_sprite.setTexture(da.m_texture);
    m_sprite.setTextureRect({ x * 64.f, y * 64.f, 64.f, 64.f });

    glm::vec2 windowSize = cro::App::getWindow().getSize();
    setPosition({ 0.f, -IconSize.y });
}

//public
void DefaultAchievements::AchievementIcon::update(float dt)
{
    static const float Speed = 240.f;
    if (m_state == ScrollIn)
    {
        move({ 0.f, Speed * dt });
        if (getPosition().y > 0.f)
        {
            setPosition({ 0.f, 0.f });
            m_state = Paused;
        }
    }
    else if(m_state == Paused)
    {
        m_pauseTime -= dt;
        if (m_pauseTime < 0)
        {
            m_state = ScrollOut;
        }
    }
    else
    {
        move({ 0.f, -Speed * dt });
        if (getPosition().y < -IconSize.y)
        {
            m_complete = true;
        }
    }
}

//private
void DefaultAchievements::AchievementIcon::draw(const glm::mat4& parentTx)
{
    auto tx = getTransform() * parentTx;

    m_background.draw(tx);
    m_sprite.draw(tx);
    m_text.draw(tx);
    m_titleText.draw(tx);
}
