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

#include "VoxelState.hpp"
#include "../StateIDs.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/MeshBuilder.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    bool showDebug = false;
}

VoxelState::VoxelState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State(ss, ctx),
    m_scene(ctx.appInstance.getMessageBus())
{
    buildScene();

    registerWindow([&]()
        {
            if (ImGui::Begin("Stats"))
            {
                
            }
            ImGui::End();
        });
}

//public
bool VoxelState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_l:
        {
        }
        break;
        case SDLK_p:

            break;
        case SDLK_BACKSPACE:
            requestStackPop();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    m_scene.forwardEvent(evt);

    return false;
}

void VoxelState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool VoxelState::simulate(float dt)
{
    m_scene.simulate(dt);

    return false;
}

void VoxelState::render()
{
    m_scene.render();
}

//private
void VoxelState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CallbackSystem>(mb);
    //m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);

    //m_environmentMap.loadFromFile("assets/images/hills.hdr");
}