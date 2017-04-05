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

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/systems/MeshRenderer.hpp>

#include <crogine/graphics/CubeBuilder.hpp>
#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/SphereBuilder.hpp>

namespace
{

}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context)
	: cro::State    (stack, context),
    m_meshRenderer  (nullptr)
{
    //TODO launch load screen
    //add systems to scene
    addSystems();
    //load assets (textures, shaders, models etc)
    loadAssets();
    //create some entities
    createScene();
    //TODO quit load screen
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
    m_meshRenderer->render();
}

//private
void MenuState::addSystems()
{
    m_meshRenderer = &m_scene.addSystem<cro::MeshRenderer>(m_scene.getDefaultCamera());
    m_scene.addSystem<RotateSystem>();
}

void MenuState::loadAssets()
{
    //load shader and material here
    auto& shader = m_shaderResource.get(0); //falls back to default
    auto& material = m_materialResource.add(0, shader);
    material.setProperty("u_colour", cro::Colour(0.4f, 1.f, 0.6f, 1.f));

    m_textureResource.setFallbackColour(cro::Colour(1.f, 1.f, 1.f));
    auto& tex = m_textureResource.get("assets/test.png");
    material.setProperty("u_texture", tex);
    auto& otherTex = m_textureResource.get("assets/blort.jpg");
    otherTex.setSmooth(true);
    material.setProperty("u_otherTexture", otherTex);

    auto& ballMat = m_materialResource.add(1, shader);
    ballMat.setProperty("u_colour", cro::Colour(1.f, 1.f, 1.f));
    ballMat.setProperty("u_texture", m_textureResource.get("assets/sphere_test.png"));

    //test the mesh builders
    cro::CubeBuilder cb;
    m_meshResource.loadMesh(cb, cro::Mesh::CubeMesh);

    cro::QuadBuilder qb({ 0.5f, 1.f });
    m_meshResource.loadMesh(qb, cro::Mesh::QuadMesh);

    cro::SphereBuilder sb(0.3f, 6);
    m_meshResource.loadMesh(sb, cro::Mesh::SphereMesh);
}

void MenuState::createScene()
{
    auto material = m_materialResource.get(0);    
    
    cro::Entity ent = m_scene.createEntity();
    auto& tx = ent.addComponent<cro::Transform>();
    tx.setPosition({ -1.2f, 0.1f, -4.6f });
    tx.rotate({ 0.5f, 1.f, 0.3f }, 1.2f);
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
    tx3.move({ 0.f, 0.f, -3.f });
    ent.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::SphereMesh), m_materialResource.get(1));
    auto& r2 = ent.addComponent<Rotator>();
    r2.speed = -0.67f;
    r2.axis.x = 0.141f;
    r2.axis.z = 0.141f;
}