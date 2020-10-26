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

#include "BackgroundDirector.hpp"
#include "Messages.hpp"
#include "ResourceIDs.hpp"
#include "RandomTranslation.hpp"

#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/util/Random.hpp>

void BackgroundDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::BackgroundSystem)
    {
        const auto& data = msg.getData<BackgroundEvent>();
        if (data.type == BackgroundEvent::ModeChanged)
        {
            bool start = (data.value != 0.f);

            cro::Command cmd;
            cmd.targetFlags = CommandID::RockParticles;
            cmd.action = [start](cro::Entity entity, float)
            {
                auto& ps = entity.getComponent<cro::ParticleEmitter>();
                if (start)
                {
                    ps.start();
                }
                else
                {
                    ps.stop();
                }
            };
            sendCommand(cmd);

            cmd.targetFlags = CommandID::SnowParticles;
            cmd.action = [start](cro::Entity entity, float)
            {
                auto& rt = entity.getComponent<RandomTranslation>();
                if (start)
                {
                    for (auto& p : rt.translations)
                    {
                        p.x = cro::Util::Random::value(-5.5f, 5.1f);
                        p.y = 3.1f;
                        p.z = -9.2f;
                    }
                }
                else
                {
                    for (auto& p : rt.translations)
                    {
                        p.x = cro::Util::Random::value(-3.5f, 12.1f);
                        p.y = 3.1f;
                        p.z = -9.2f;
                    }
                }
            };
            sendCommand(cmd);

        }
        else if (data.type == BackgroundEvent::SpeedChange)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::SnowParticles;
            cmd.action = [=](cro::Entity entity, float)
            {
                entity.getComponent<cro::ParticleEmitter>().settings.initialVelocity.x = (data.value * -11.f);
            };
            sendCommand(cmd);
        }
    }
}