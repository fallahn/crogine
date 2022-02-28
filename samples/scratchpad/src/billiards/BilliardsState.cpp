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

#include "BilliardsState.hpp"

BilliardsState::BilliardsState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State(ss, ctx),
    m_scene(ctx.appInstance.getMessageBus())
{
    addSystems();
    buildScene();
}

//public
bool BilliardsState::handleEvent(const cro::Event& evt)
{
    m_scene.forwardEvent(evt);
    return false;
}

void BilliardsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool BilliardsState::simulate(float dt)
{
    m_scene.simulate(dt);
    return false;
}

void BilliardsState::render()
{
    m_scene.render();
}

//private
void BilliardsState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

}

void BilliardsState::buildScene()
{

}