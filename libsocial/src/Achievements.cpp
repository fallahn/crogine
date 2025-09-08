/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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

#include "../include/Achievements.hpp"

#include <crogine/detail/Assert.hpp>

AchievementImpl* Achievements::m_impl = nullptr;
bool Achievements::m_active = true;

bool Achievements::init(AchievementImpl& impl)
{
    m_impl = &impl;
    return m_impl->init();
}

void Achievements::update()
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    m_impl->update();
}

void Achievements::shutdown()
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    m_impl->shutdown();
}

void Achievements::awardAchievement(const std::string& name)
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    if (m_active)
    {
        m_impl->awardAchievement(name);
    }
}

const AchievementData* Achievements::getAchievement(const std::string& name)
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    return m_impl->getAchievement(name);
}

AchievementImage Achievements::getIcon(const std::string& name)
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    return m_impl->getIcon(name);
}

void Achievements::setStat(const std::string& name, float value)
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    if (m_active)
    {
        m_impl->setStat(name, value);
    }
}

void Achievements::setStat(const std::string& name, std::int32_t value)
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    if (m_active)
    {
        m_impl->setStat(name, value);
    }
}

float Achievements::incrementStat(const std::string& name, std::int32_t value)
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    if (m_active)
    {
        return m_impl->incrementStat(name, value);
    }
    return 0.f;
}

float Achievements::incrementStat(const std::string& name, float value)
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    if (m_active)
    {
        return m_impl->incrementStat(name, value);
    }
    return 0.f;
}

const StatData* Achievements::getStat(const std::string& name)
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    return m_impl->getStat(name);
}

void Achievements::setAvgStat(const std::string& s, float value, float)
{
    CRO_ASSERT(m_impl, "Achievements have not been initialised!");
    static constexpr float alpha = 0.9f; //the bigger this is the smaller the window

    auto* stat = m_impl->getStat(s);
    if (stat)
    {
        auto v = stat->value;
        v = (alpha * value) + (1.f - alpha) * v;
        m_impl->setStat(s, v);
    }
}

void Achievements::setActive(bool active)
{
    m_active = active;
}