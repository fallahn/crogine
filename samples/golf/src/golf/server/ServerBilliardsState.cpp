/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include "ServerBilliardsState.hpp"

using namespace sv;

BilliardsState::BilliardsState(SharedData& sd)
    : m_returnValue(StateID::Billiards),
    m_sharedData(sd),
    m_scene(sd.messageBus, 512)
{
    //TODO validate table data
    initScene();
    buildWorld();

    LOG("Entered Billiards state", cro::Logger::Type::Info);
}

//public
void BilliardsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

void BilliardsState::netEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {

    }
}

void BilliardsState::netBroadcast()
{

}

std::int32_t BilliardsState::process(float dt)
{
    m_scene.simulate(dt);
    return m_returnValue;
}

//private
void BilliardsState::initScene()
{

}

void BilliardsState::buildWorld()
{

}