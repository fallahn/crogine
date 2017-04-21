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

#include "MainState.hpp"
#include "RotateSystem.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/SceneRenderer.hpp>
#include <crogine/ecs/systems/MeshSorter.hpp>

#include <crogine/graphics/SphereBuilder.hpp>
#include <crogine/graphics/QuadBuilder.hpp>

namespace
{
    namespace MaterialID
    {
        enum
        {
            Moon,
            Stars
        };
    }
}

MainState::MainState(cro::StateStack& stack, cro::State::Context context)
    : cro::State        (stack, context),
    m_backgroundRenderer(nullptr),
    m_currentMenu       (nullptr),
    m_commandSystem     (nullptr)
{
    addSystems();
    loadAssets();
    createScene();
    //context.appInstance.setClearColour(cro::Colour::Red());
}

//public
bool MainState::handleEvent(const cro::Event& evt)
{
    return true;
}

void MainState::handleMessage(const cro::Message& msg)
{
    m_backgroundScene.forwardMessage(msg);
}

bool MainState::simulate(cro::Time dt)
{
    m_backgroundScene.simulate(dt);
    return true;
}

void MainState::render() const
{
    m_backgroundRenderer->render();
}

//private
void MainState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_backgroundRenderer = &m_backgroundScene.addSystem<cro::SceneRenderer>(mb, m_backgroundScene.getDefaultCamera());
    m_backgroundScene.addSystem<cro::MeshSorter>(mb, *m_backgroundRenderer);
    m_backgroundScene.addSystem<RotateSystem>(mb);
}

void MainState::loadAssets()
{
    cro::int32 shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::NormalMap);
    auto& moonMaterial = m_materialResource.add(MaterialID::Moon, m_shaderResource.get(shaderID));
    moonMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/rock_diffuse.png"));
    moonMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/rock_normal.png"));
    moonMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/rock_mask.png"));

    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap);
    auto& starTexture = m_textureResource.get("assets/materials/stars.png");
    starTexture.setRepeated(true);
    auto& skyMaterial = m_materialResource.add(MaterialID::Stars, m_shaderResource.get(shaderID));
    skyMaterial.setProperty("u_diffuseMap", starTexture);
    skyMaterial.blendMode = cro::Material::BlendMode::Additive;

    cro::SphereBuilder sb(2.2f, 6);
    m_meshResource.loadMesh(sb, cro::Mesh::SphereMesh);

    cro::QuadBuilder qb(glm::vec2(16.f, 9.f), glm::vec2(16.f / 9.f, 1.f));
    m_meshResource.loadMesh(qb, cro::Mesh::QuadMesh);
}

void MainState::createScene()
{
    //-----background-----//
    //create moon
    auto entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 2.3f, -0.7f, -6.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::SphereMesh), m_materialResource.get(MaterialID::Moon));
    auto& moonRotator = entity.addComponent<Rotator>();
    moonRotator.speed = 0.03f;
    moonRotator.axis.y = 0.2f;
    moonRotator.axis.z = -0.2f;

    //create stars
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -18.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(MaterialID::Stars));

    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -17.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(MaterialID::Stars));

    //entity = m_backgroundScene.createEntity();
    //auto& tx4 = entity.addComponent<cro::Transform>();
    //tx4.move({ 0.f, 0.4f, 1.f });
    //tx4.rotate({ 1.f, 0.f, 0.f }, -0.1f);
    //entity.addComponent<cro::Camera>();
    //m_backgroundRenderer->setActiveCamera(entity);

    //-----menus-----//
}