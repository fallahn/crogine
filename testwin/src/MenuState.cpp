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
#include <crogine/ecs/components/CommandID.hpp>
#include <crogine/ecs/systems/MeshSorter.hpp>
#include <crogine/ecs/systems/SceneRenderer.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>

#include <crogine/graphics/CubeBuilder.hpp>
#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/SphereBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>

namespace
{
    enum Command
    {
        Keyboard = 0x1,
        Touch = 0x2,
        Drag = 0x4
    };

    enum Input
    {
        Left = 0x1,
        Right = 0x2,
        Up = 0x4,
        Down = 0x8,
        FingerDown = 0x10
    };
    cro::uint16 input = 0;
    glm::vec2 movement;
}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context)
	: cro::State    (stack, context),
    m_scene         (context.appInstance.getMessageBus()),
    m_sceneRenderer (nullptr),
    m_spriteRenderer(nullptr),
    m_commandSystem (nullptr)
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
    static glm::vec2 lastFingerPos(0.5f, 0.5f);
    switch (evt.type)
    {
    default: break;
    case SDL_KEYDOWN:
        {
            switch (evt.key.keysym.sym)
            {
            default: break;
            case SDLK_LEFT:
                input |= Left;
                break;
            case SDLK_RIGHT:
                input |= Right;
                break;
            }
        }
        break;
    case SDL_KEYUP:
        {
            switch (evt.key.keysym.sym)
            {
            default: break;
            case SDLK_LEFT:
                input &= ~Left;
                break;
            case SDLK_RIGHT:
                input &= ~Right;
                break;
            }
        }
        break;
    case SDL_FINGERDOWN:
        {
            //cro::Command cmd;
            //cmd.targetFlags = Touch;
            //cmd.action = [](cro::Entity entity, cro::Time)
            //{
            //    auto& r = entity.getComponent<Rotator>();
            //    r.speed = -r.speed;

            //    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::Blue());
            //};
            //m_commandSystem->sendCommand(cmd);
            input |= FingerDown;
            lastFingerPos = { evt.tfinger.x, evt.tfinger.y };
        }
        break;
    case SDL_FINGERUP:
        {
            input &= ~FingerDown;
        }
        break;
    case SDL_FINGERMOTION:
        {
        
        glm::vec2 pos(evt.tfinger.x, evt.tfinger.y);
        movement = pos - lastFingerPos;
        lastFingerPos = pos;
        }
        break;
    }
	return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool MenuState::simulate(cro::Time dt)
{
    //parse input
    cro::Command cmd;
    cmd.targetFlags = Keyboard;
    if (input & Left)
    {
        cmd.action = [](cro::Entity entity, cro::Time dt)
        {
            entity.getComponent<cro::Transform>().rotate({ 0.f, 1.f, 0.f }, -0.9f * dt.asSeconds());
        };
        m_commandSystem->sendCommand(cmd);
    }
    if (input & Right)
    {
        cmd.action = [](cro::Entity entity, cro::Time dt)
        {
            entity.getComponent<cro::Transform>().rotate({ 0.f, 1.f, 0.f }, 0.9f * dt.asSeconds());
        };
        m_commandSystem->sendCommand(cmd);
    }
    
    if (input & FingerDown)
    {
        cmd.targetFlags = Drag;
        cmd.action = [](cro::Entity entity, cro::Time dt)
        {
            const float speed = 500.f;
            glm::vec3 move(movement.x, -movement.y, 0.f);
            entity.getComponent<cro::Transform>().move(move *  speed * dt.asSeconds());
        };
        m_commandSystem->sendCommand(cmd);
    }

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
    auto& mb = getContext().appInstance.getMessageBus();

    m_sceneRenderer = &m_scene.addSystem<cro::SceneRenderer>(mb, m_scene.getDefaultCamera());
    m_scene.addSystem<cro::MeshSorter>(mb, *m_sceneRenderer);
    m_spriteRenderer = &m_scene.addSystem<cro::SpriteRenderer>(mb);
    m_commandSystem = &m_scene.addSystem<cro::CommandSystem>(mb);

    m_scene.addSystem<RotateSystem>(mb);
    m_scene.addSystem<ColourSystem>(mb);
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

    cro::StaticMeshBuilder smb("assets/room_1x1.cmf");
    m_meshResource.loadMesh(smb, cro::Mesh::Count);
}

void MenuState::createScene()
{
    //----3D stuff----//
    
    auto material = m_materialResource.get(0);

    cro::Entity ent = m_scene.createEntity();
    auto& tx = ent.addComponent<cro::Transform>();
    tx.setPosition({ -1.2f, 0.1f, -4.6f });
    tx.rotate({ 0.5f, 1.f, 0.3f }, 0.6f);
    ent.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), material);
    ent.addComponent<cro::CommandTarget>().ID |= Keyboard;

    ent = m_scene.createEntity();
    auto& tx2 = ent.addComponent<cro::Transform>();
    tx2.setPosition({ 1.3f, -0.61f, -4.96f });
    //tx2.rotate({ 1.5f, 1.f, 0.03f }, 1.7f);
    tx2.scale({ 0.5f, 0.4f, 0.44f });
    ent.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::CubeMesh), material);
    auto& r = ent.addComponent<Rotator>();
    r.speed = 0.5f;
    r.axis.y = 1.f;
    ent.addComponent<cro::CommandTarget>().ID |= Touch;

    ent = m_scene.createEntity();
    ent.addComponent<cro::CommandTarget>().ID = Drag;
    auto& tx3 = ent.addComponent<cro::Transform>();
    tx3.move({ 32.f, -2.5f, -13.f });
    tx3.scale({ 0.5f, 0.5f, 0.5f });
    //tx3.rotate({ 0.f, 1.f, 0.f }, 3.14f);

    auto& model = ent.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::Count), m_materialResource.get(1));
    //model.setMaterial(1, material);
    /*auto& r2 = ent.addComponent<Rotator>();
    r2.speed = -0.67f;
    r2.axis.x = 0.141f;
    r2.axis.z = 0.141f;*/

    ent = m_scene.createEntity();
    auto& tx4 = ent.addComponent<cro::Transform>();
    tx4.move({ 0.f, 0.4f, 1.f });
    tx4.rotate({ 1.f, 0.f, 0.f }, -0.1f);
    ent.addComponent<cro::Camera>();
    m_sceneRenderer->setActiveCamera(ent);

    //------sprite stuff-----//

    auto& uiTex = m_textureResource.get("assets/interface.png");
    
    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 10.f, cro::DefaultSceneSize.y - 48.f, 0.f });
    ent.getComponent<cro::Transform>().setOrigin({ 128.f, 19.f, 0.f });
    auto& sprite = ent.addComponent<cro::Sprite>();
    //sprite.setColour(cro::Colour(1.f, 0.5f, 0.f, 1.f));
    sprite.setTexture(uiTex);
    sprite.setTextureRect({ 0.f, 210.f, 256.f, 38.f });
    auto& rr = ent.addComponent<Rotator>();
    rr.axis.z = 1.f;
    rr.speed = 3.f;

    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ cro::DefaultSceneSize.x - 120.f, 10.f, 0.f });
    auto& spr = ent.addComponent<cro::Sprite>();
    spr.setColour(cro::Colour(0.f, 0.5f, 1.f, 1.f));
    spr.setTexture(uiTex);
    spr.setTextureRect({ 6.f, 136.f, 110.f, 66.f });
    ent.addComponent<ColourChanger>();

    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 20.f, 10.f, 0.f });
    auto& spr2 = ent.addComponent<cro::Sprite>();
    spr2.setTexture(uiTex);
    spr2.setTextureRect({ 0.f, 0.f, 136.f, 136.f });

    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ cro::DefaultSceneSize.x - 80.f, 82.f, 0.f });
    auto& spr3 = ent.addComponent<cro::Sprite>();
    spr3.setColour(cro::Colour(1.f, 0.5f, 0.5f, 1.f));
    spr3.setTexture(uiTex);
    spr3.setTextureRect({ 180.f, 0.f, 76.f, 214.f });
    ent.addComponent<ColourChanger>();
}
