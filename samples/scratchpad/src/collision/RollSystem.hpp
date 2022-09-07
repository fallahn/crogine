/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/ecs/System.hpp>
#include <crogine/gui/GuiClient.hpp>

class btCollisionWorld;
class btCollisionDispatcher;
class btPairCachingGhostObject;

struct Roller final
{
    glm::vec3 resetPosition = glm::vec3(0.f);
    
    glm::vec3 velocity = glm::vec3(0.f);
    glm::vec3 prevVelocity = glm::vec3(0.f);
    glm::vec3 prevPosition = glm::vec3(0.f);

    float mass = 60.f;
    float friction = 0.98f;

    enum
    {
        Air, Roll, Sleep
    }state = Air;
};

class RollingSystem final : public cro::System, public cro::GuiClient
{
public:
    RollingSystem(cro::MessageBus&, std::unique_ptr<btCollisionWorld>&, std::unique_ptr<btCollisionDispatcher>&);

    void process(float) override;


private:
    std::unique_ptr<btCollisionWorld>& m_collisionWorld;
    std::unique_ptr<btCollisionDispatcher>& m_collisionDispatcher;
};