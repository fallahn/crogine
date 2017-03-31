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

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/systems/MeshRenderer.hpp>


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
    m_meshRenderer = &m_scene.addSystem<cro::MeshRenderer>();
}

void MenuState::loadAssets()
{
    //load shader and material here
    auto& shader = m_shaderResource.get(0); //falls back to default
    m_materialResource.add(0, shader);
}

void MenuState::createScene()
{
    cro::Entity ent = m_scene.createEntity();
    auto& tx = ent.addComponent<cro::Transform>();
    tx.setPosition({ 20.f, 100.f, 0.f });

    auto material = m_materialResource.get(0);
    
    auto& quad = ent.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::Quad), material);
    quad.setMaterial(0, material);
}