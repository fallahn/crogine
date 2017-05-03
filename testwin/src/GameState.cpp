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

#include "GameState.hpp"
#include "ResourceIDs.hpp"
#include "BackgroundShader.hpp"
#include "BackgroundController.hpp"
#include "PostRadial.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/graphics/QuadBuilder.hpp>

#include <crogine/ecs/systems/MeshSorter.hpp>
#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/SceneRenderer.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

namespace
{
    BackgroundController* cnt = nullptr;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_scene         (context.appInstance.getMessageBus()),
    m_commandSystem (nullptr)
{
    addSystems();
    loadAssets();
    createScene();

    //context.appInstance.setClearColour(cro::Colour::White());
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_SPACE)
        {
            cnt->setMode(BackgroundController::Mode::Shake);
        }
        else if (evt.key.keysym.sym == SDLK_RETURN)
        {
            cnt->setMode(BackgroundController::Mode::Scroll);
            cnt->setScrollSpeed(0.2f);
        }
    }
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool GameState::simulate(cro::Time dt)
{
    m_scene.simulate(dt);
    return true;
}

void GameState::render()
{
    m_scene.render();
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::SceneGraph>(mb);
    auto& sceneRenderer = m_scene.addSystem<cro::SceneRenderer>(mb, m_scene.getDefaultCamera());
    m_scene.addSystem<cro::MeshSorter>(mb, sceneRenderer);
    cnt = &m_scene.addSystem<BackgroundController>(mb);
    cnt->setScrollSpeed(0.2f);

    m_scene.addPostProcess<PostRadial>();
}

void GameState::loadAssets()
{
    m_shaderResource.preloadFromString(Shaders::Background::Vertex, Shaders::Background::Fragment, ShaderID::Background);
    auto& farTexture = m_textureResource.get("assets/materials/background_far.png");
    farTexture.setRepeated(true);
    farTexture.setSmooth(true);
    auto& farMaterial = m_materialResource.add(MaterialID::GameBackgroundFar, m_shaderResource.get(ShaderID::Background));
    farMaterial.setProperty("u_diffuseMap", farTexture);

    auto& midTexture = m_textureResource.get("assets/materials/background_mid.png");
    midTexture.setRepeated(true);
    midTexture.setSmooth(true);
    auto& midMaterial = m_materialResource.add(MaterialID::GameBackgroundMid, m_shaderResource.get(ShaderID::Background));
    midMaterial.setProperty("u_diffuseMap", midTexture);
    midMaterial.blendMode = cro::Material::BlendMode::Alpha;

    auto& nearTexture = m_textureResource.get("assets/materials/background_near.png");
    nearTexture.setRepeated(true);
    nearTexture.setSmooth(true);
    auto& nearMaterial = m_materialResource.add(MaterialID::GameBackgroundNear, m_shaderResource.get(ShaderID::Background));
    nearMaterial.setProperty("u_diffuseMap", nearTexture);
    nearMaterial.blendMode = cro::Material::BlendMode::Alpha;

    cro::QuadBuilder qb({ 21.3f, 7.2f });
    m_meshResource.loadMesh(qb, MeshID::GameBackground);
}

void GameState::createScene()
{
    //background layers
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -18.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::GameBackground), m_materialResource.get(MaterialID::GameBackgroundFar));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -14.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::GameBackground), m_materialResource.get(MaterialID::GameBackgroundMid));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -11.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::GameBackground), m_materialResource.get(MaterialID::GameBackgroundNear));
    entity.addComponent<BackgroundComponent>();


    /*auto ent = m_scene.createEntity();
    auto& tx4 = ent.addComponent<cro::Transform>();
    ent.addComponent<cro::Camera>();
    m_scene.getSystem<cro::SceneRenderer>().setActiveCamera(ent);
    auto& r = ent.addComponent<Rotator>();
    r.axis.x = 1.f;
    r.speed = 0.6f;*/
}