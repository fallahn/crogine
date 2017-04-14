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

#include "MenuState.hpp"
#include "RotateSystem.hpp"
#include "ColourSystem.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/systems/MeshSorter.hpp>
#include <crogine/ecs/systems/SceneRenderer.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>

#include <crogine/graphics/CubeBuilder.hpp>
#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/SphereBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>

namespace
{

}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context)
	: cro::State    (stack, context),
    m_sceneRenderer (nullptr),
    m_spriteRenderer(nullptr)
{
    //TODO launch load screen
    //add systems to scene
    addSystems();
    //load assets (textures, shaders, models etc)
    loadAssets();
    //create some entities
    createScene();
    //TODO quit load screen

    context.appInstance.setClearColour(cro::Colour(0.2f, 0.2f, 0.26f));
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    //TODO apply input mask to any systems which want to read it
	return true;
}

bool MenuState::simulate(cro::Time dt)
{
    m_scene.simulate(dt);
	return true;
}

void MenuState::render() const
{
	//draw any renderable systems
    m_sceneRenderer->render();
    m_spriteRenderer->render();    
}

//private
void MenuState::addSystems()
{
    m_sceneRenderer = &m_scene.addSystem<cro::SceneRenderer>(m_scene.getDefaultCamera());
    m_scene.addSystem<cro::MeshSorter>(*m_sceneRenderer);
    m_spriteRenderer = &m_scene.addSystem<cro::SpriteRenderer>();

    m_scene.addSystem<RotateSystem>();
    m_scene.addSystem<ColourSystem>();
}

void MenuState::loadAssets()
{
    //load shader and material here
    auto& shader = m_shaderResource.get(0); //falls back to default
    auto& material = m_materialResource.add(0, shader);

    m_textureResource.setFallbackColour(cro::Colour(1.f, 1.f, 1.f));
    auto& tex = m_textureResource.get("assets/test.png");
    material.setProperty("u_diffuseMap", tex);
    auto& otherTex = m_textureResource.get("assets/blort.jpg");
    otherTex.setSmooth(true);
    //material.setProperty("u_normalMap", otherTex);

    auto& ballMat = m_materialResource.add(1, shader);
    ballMat.setProperty("u_diffuseMap", m_textureResource.get("assets/sphere_test.png"));

    //test the mesh builders
    cro::CubeBuilder cb;
    m_meshResource.loadMesh(cb, cro::Mesh::CubeMesh);

    cro::QuadBuilder qb({ 0.5f, 1.f });
    m_meshResource.loadMesh(qb, cro::Mesh::QuadMesh);

    cro::SphereBuilder sb(0.3f, 6);
    m_meshResource.loadMesh(sb, cro::Mesh::SphereMesh);

    cro::StaticMeshBuilder smb("assets/beeble.cmf");
    m_meshResource.loadMesh(smb, cro::Mesh::Count);
}

void MenuState::createScene()
{
    auto material = m_materialResource.get(0);

    cro::Entity ent = m_scene.createEntity();
    auto& tx = ent.addComponent<cro::Transform>();
    tx.setPosition({ -1.2f, 0.1f, -4.6f });
    tx.rotate({ 0.5f, 1.f, 0.3f }, 0.6f);
    ent.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), material);


    ent = m_scene.createEntity();
    auto& tx2 = ent.addComponent<cro::Transform>();
    tx2.setPosition({ 1.3f, -0.61f, -4.96f });
    //tx2.rotate({ 1.5f, 1.f, 0.03f }, 1.7f);
    tx2.scale({ 0.5f, 0.4f, 0.44f });
    ent.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::CubeMesh), material);
    auto& r = ent.addComponent<Rotator>();
    r.speed = 0.5f;
    r.axis.y = 1.f;

    ent = m_scene.createEntity();
    auto& tx3 = ent.addComponent<cro::Transform>();
    tx3.move({ 0.f, 0.f, -13.f });
    tx3.scale({ 0.5f, 0.5f, 0.5f });

    auto& model = ent.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::Count), m_materialResource.get(1));
    model.setMaterial(1, material);
    auto& r2 = ent.addComponent<Rotator>();
    r2.speed = -0.67f;
    r2.axis.x = 0.141f;
    r2.axis.z = 0.141f;

    ent = m_scene.createEntity();
    auto& tx4 = ent.addComponent<cro::Transform>();
    tx4.move({ 0.f, 0.4f, 1.f });
    tx4.rotate({ 1.f, 0.f, 0.f }, -0.1f);
    ent.addComponent<cro::Camera>();
    m_sceneRenderer->setActiveCamera(ent);

    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 40.f, 40.f, 0.f });
    auto& sprite = ent.addComponent<cro::Sprite>();
    sprite.setColour(cro::Colour(1.f, 0.5f, 0.f, 1.f));
    sprite.setTexture(m_textureResource.get("g"));
    sprite.setSize({ 100.f, 50.f });

    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 200.f, 10.f, 0.f });
    auto& spr = ent.addComponent<cro::Sprite>();
    spr.setColour(cro::Colour(0.f, 0.5f, 1.f, 1.f));
    spr.setTexture(m_textureResource.get("assets/test.png"));
    spr.setSize({ 200.f, 50.f });
    ent.addComponent<ColourChanger>();

    ent = m_scene.createEntity();
    auto& tx5 = ent.addComponent<cro::Transform>();
    tx5.setPosition({ 220.f, 200.f, 0.f });
    tx5.setScale({ 0.5, 0.5f, 0.5f });
    tx5.setRotation({ 0.f, 0.5f, 0.f });
    auto& spr2 = ent.addComponent<cro::Sprite>();
    //spr2.setColour(cro::Colour::Yellow());
    //spr2.setSize({ 220.2f, 100.4f });
    spr2.setTexture(m_textureResource.get("assets/sphere_test.png"));

    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 500.f, 140.f, 0.f });
    auto& spr3 = ent.addComponent<cro::Sprite>();
    spr3.setColour(cro::Colour(1.f, 0.5f, 0.5f, 1.f));
    spr3.setTexture(m_textureResource.get("g"));
    spr3.setSize({ 10.f, 150.f });
    auto& r3 = ent.addComponent<Rotator>();
    r3.axis.z = 1.f;
    r3.speed = -4.1f;
    ent.addComponent<ColourChanger>();
}
