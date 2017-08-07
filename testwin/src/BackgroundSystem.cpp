/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "BackgroundSystem.hpp"
#include "Messages.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/gui/Gui.hpp>

#include <array>

namespace
{
    std::array<float, 200u> noiseTable;
}


BackgroundSystem::BackgroundSystem(cro::MessageBus& mb)
    : cro::System       (mb, typeid(BackgroundSystem)),
    m_speed             (0.f),
    m_currentSpeed      (0.f),
    m_currentMode       (Mode::Scroll),
    m_currentIndex      (0),
    m_colourAngle       (0.f),
    m_currentColourAngle(0.f)
{
    requireComponent<BackgroundComponent>();
    requireComponent<cro::Model>();

    for (auto& t : noiseTable)
    {
        t = cro::Util::Random::value(-0.0008f, 0.0008f);
    }

    /*registerStatusControls([&]()
    {
        static bool bossMode = false;
        bool lastMode = bossMode;
        cro::Nim::checkbox("Boss Mode", &bossMode);
        if (lastMode != bossMode)
        {
            if (bossMode)
            {
                setMode(Mode::Shake);
            }
            else
            {
                setMode(Mode::Scroll);
                setScrollSpeed(0.2f);
            }
        }
    });*/
}

//public
void BackgroundSystem::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;

    case MessageID::GameMessage:
    {
        const auto& data = msg.getData<GameEvent>();
        switch (data.type)
        {
        default: break;
        case GameEvent::RoundStart:
            setMode(Mode::Scroll);
            setScrollSpeed(0.2f);
            break;
        case GameEvent::BossStart:
            setMode(Mode::Shake);
            break;
        case GameEvent::GameOver:
        case GameEvent::RoundEnd:
            setMode(Mode::Scroll);
            setScrollSpeed(0.001f);
            break;
        }    
    }
        break;
    }
}

void BackgroundSystem::process(cro::Time dt)
{
    float dtSec = dt.asSeconds();

    m_currentColourAngle += ((m_colourAngle - m_currentColourAngle) * dtSec);

    m_currentSpeed += ((m_speed - m_currentSpeed) * dtSec);

    m_offset.x += m_currentSpeed * dtSec;

    if(m_currentMode == Mode::Shake)
    {
        m_offset.x += noiseTable[m_currentIndex];

        auto* msg = postMessage<BackgroundEvent>(MessageID::BackgroundSystem);
        msg->type = BackgroundEvent::Shake;
        msg->value = -noiseTable[m_currentIndex] * 100.f;

        noiseTable[m_currentIndex] = -noiseTable[m_currentIndex]; //averages out overall movement so it doesn't drift over time
        m_currentIndex = (m_currentIndex + 1) % noiseTable.size();
    }

    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        //TODO... aren't the shader properties stored in the component?
        auto& model = entity.getComponent<cro::Model>();
        model.setMaterialProperty(0, "u_textureOffset", m_offset);
        model.setMaterialProperty(0, "u_colourAngle", m_currentColourAngle);
    }
}


//private
void BackgroundSystem::setScrollSpeed(float speed)
{
    m_speed = speed;

    auto* msg = postMessage<BackgroundEvent>(MessageID::BackgroundSystem);
    msg->type = BackgroundEvent::SpeedChange;
    msg->value = m_speed;
}

void BackgroundSystem::setColourAngle(float angle)
{
    m_colourAngle = angle;
}

void BackgroundSystem::setMode(Mode mode)
{
    m_currentMode = mode;
    if (mode == Mode::Shake)
    {
        setScrollSpeed(0.f);
    }
    auto* msg = postMessage<BackgroundEvent>(MessageID::BackgroundSystem);
    msg->type = BackgroundEvent::ModeChanged;
    msg->value = (mode == Mode::Shake) ? 1.f : 0.f;
}