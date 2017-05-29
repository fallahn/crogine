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
#include "Messages.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/IqmBuilder.hpp>

#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/CommandID.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Constants.hpp>

#include <glm/gtx/norm.hpp>

namespace
{
    cro::int32 rowCount = 3;
    cro::int32 rowWidth = 6;

    cro::uint8 camFlags = 0;
    enum
    {
        Up = 0x1, Down = 0x2, Left = 0x4, Right = 0x8
    };

    cro::Skeleton batCatSkeleton;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_scene         (context.appInstance.getMessageBus()),
    m_overlayScene  (context.appInstance.getMessageBus()),
    m_commandSystem (nullptr)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
    });
    context.appInstance.setClearColour(cro::Colour::White());
    //context.mainWindow.setVsyncEnabled(false);

    updateView();

    auto* msg = getContext().appInstance.getMessageBus().post<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RoundStart;
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    /*auto rebuild = [&](cro::int32 target)
    {
        cro::Command cmd;
        cmd.targetFlags = target;
        cmd.action = [&](cro::Entity entity, cro::Time)
        {
            m_scene.destroyEntity(entity);
        };
        m_commandSystem->sendCommand(cmd);

        for (auto i = 0; i < rowCount; ++i)
        {
            buildRow(i);
        }
    };*/

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        /*case SDLK_UP:
        {
            cro::int32 target = (1 << (rowCount * rowWidth));
            auto old = rowCount;
            rowCount = std::min(rowCount + 1, 11);
            if(old != rowCount) rebuild(target);
        }
            break;
        case SDLK_DOWN:
        {
            cro::int32 target = (1 << (rowCount * rowWidth));
            auto old = rowCount;
            rowCount = std::max(1, rowCount - 1);
            if(old != rowCount) rebuild(target);
        }
            break;
        case SDLK_LEFT:
        {
            cro::int32 target = (1 << (rowCount * rowWidth));
            auto old = rowWidth;
            rowWidth = std::max(2, rowWidth - 1);
            if(old != rowWidth) rebuild(target);
        }
            break;
        case SDLK_RIGHT:
        {
            cro::int32 target = (1 << (rowCount * rowWidth));
            auto old = rowWidth;
            rowWidth = std::min(rowWidth + 1, 8);
            if(old != rowWidth) rebuild(target);
        }
            break;*/
        case SDLK_w:
            //camFlags &= ~Up;
            break;
        case SDLK_s:
            //camFlags &= ~Down;
            break;
        case SDLK_a:
            //camFlags &= ~Left;
            break;
        case SDLK_d:
            //camFlags &= ~Right;
            break;
        }
    }

    else if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default:break;
        case SDLK_w:
            //camFlags |= Up;
            break;
        case SDLK_s:
            //camFlags |= Down;
            break;
        case SDLK_a:
            //camFlags |= Left;
            break;
        case SDLK_d:
            //camFlags |= Right;
            break;
        }
    }

    //else if (evt.type == SDL_MOUSEWHEEL)
    //{
    //    //DPRINT("Wheel", std::to_string(evt.wheel.y));
    //    auto amount = -evt.wheel.y;
    //    cro::Command cmd;
    //    cmd.targetFlags = CommandID::Camera;
    //    cmd.action = [amount](cro::Entity entity, cro::Time)
    //    {
    //        entity.getComponent<cro::Transform>().move({ 0.f, 0.f, static_cast<float>(amount) });
    //    };
    //    m_commandSystem->sendCommand(cmd);
    //}

    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);

    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }
}

bool GameState::simulate(cro::Time dt)
{
    glm::vec3 camVec;
    if (camFlags & Up) camVec.y = 1.f;
    if (camFlags & Down) camVec.y -= 1.f;
    if (camFlags & Left) camVec.x -= 1.f;
    if (camFlags & Right) camVec.x += 1.f;
    if (glm::length2(camVec) > 0)
    {
        camVec = glm::normalize(camVec) * 0.5f;
        cro::Command cmd;
        cmd.targetFlags = CommandID::Camera;
        cmd.action = [camVec](cro::Entity entity, cro::Time dt)
        {
            auto& tx = entity.getComponent<cro::Transform>();
            tx.move(camVec * tx.getPosition().z * dt.asSeconds());
        };
        m_commandSystem->sendCommand(cmd);
    }

    m_scene.simulate(dt);
    m_overlayScene.simulate(dt);
    return true;
}

void GameState::render()
{
    m_scene.render();
    m_overlayScene.render();
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::SceneGraph>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
    m_scene.addSystem<cro::ParticleSystem>(mb);
    m_commandSystem = &m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);

    m_overlayScene.addSystem<cro::TextRenderer>(mb);
    m_overlayScene.addSystem<cro::SceneGraph>(mb);
}

void GameState::loadAssets()
{
    //auto shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseColour | cro::ShaderResource::DiffuseMap);

    //auto& greenOne = m_materialResource.add(MaterialID::GreenOne, m_shaderResource.get(shaderID));
    //greenOne.setProperty("u_colour", cro::Colour(cro::uint8(21), 178u, 55u));
    //greenOne.setProperty("u_diffuseMap", m_textureResource.get("assets/square.png"));

    //auto& greenTwo = m_materialResource.add(MaterialID::GreenTwo, m_shaderResource.get(shaderID));
    //greenTwo.setProperty("u_colour", cro::Colour(cro::uint8(127), 206u, 61u));
    //greenTwo.setProperty("u_diffuseMap", m_textureResource.get("assets/square.png"));

    //auto& brown = m_materialResource.add(MaterialID::Brown, m_shaderResource.get(shaderID));
    //brown.setProperty("u_colour", cro::Colour(cro::uint8(199), 66u, 9u));
    //brown.setProperty("u_diffuseMap", m_textureResource.get("assets/square.png"));

    //auto& red = m_materialResource.add(MaterialID::Red, m_shaderResource.get(shaderID));
    //red.setProperty("u_colour", cro::Colour(cro::uint8(140), 30u, 18u));
    //red.setProperty("u_diffuseMap", m_textureResource.get("assets/square.png"));

    //auto& blue = m_materialResource.add(MaterialID::Blue, m_shaderResource.get(shaderID));
    //blue.setProperty("u_colour", cro::Colour(cro::uint8(18), 105u, 142u));
    //blue.setProperty("u_diffuseMap", m_textureResource.get("assets/square.png"));

    //cro::QuadBuilder qb({ 1.f, 1.f });
    //m_meshResource.loadMesh(qb, cro::Mesh::QuadMesh);


    auto shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap);

    auto& roomOne = m_materialResource.add(MaterialID::RoomOne, m_shaderResource.get(shaderID));
    roomOne.setProperty("u_diffuseMap", m_textureResource.get("assets/textures/room1x1.png"));

    auto& roomTwo = m_materialResource.add(MaterialID::RoomTwo, m_shaderResource.get(shaderID));
    roomTwo.setProperty("u_diffuseMap", m_textureResource.get("assets/textures/room2x1.png"));

    cro::StaticMeshBuilder r1("assets/models/room1x1.cmf");
    m_meshResource.loadMesh(MeshID::RoomOne, r1);

    cro::StaticMeshBuilder r2("assets/models/room2x1.cmf");
    m_meshResource.loadMesh(MeshID::RoomTwo, r2);

    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap /*| cro::ShaderResource::NormalMap*/ | cro::ShaderResource::Skinning);
    auto& batMat = m_materialResource.add(MaterialID::BatCat, m_shaderResource.get(shaderID));
    auto& batDiff = m_textureResource.get("assets/textures/batcat_diffuse.png");
    batDiff.setRepeated(true);
    batMat.setProperty("u_diffuseMap", batDiff);
    /*auto& batMask = m_textureResource.get("assets/textures/batcat_mask.png");
    batMask.setRepeated(true);
    batMat.setProperty("u_maskMap", batMask);
    auto& batNormal = m_textureResource.get("assets/textures/batcat_normal.png");
    batNormal.setRepeated(true);
    batMat.setProperty("u_normalMap", batNormal);*/

    cro::IqmBuilder catBuilder("assets/models/batcat.iqm");
    m_meshResource.loadMesh(MeshID::BatCat, catBuilder);
    batCatSkeleton = catBuilder.getSkeleton();
    batCatSkeleton.currentAnimation = AnimationID::BatCat::Idle;
    batCatSkeleton.animations[AnimationID::BatCat::Idle].playing = true;
}

void GameState::createScene()
{
    //for (auto i = 0; i < rowCount; ++i)
    //{
    //    buildRow(i);
    //}

    //dat cat man
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::BatCat), m_materialResource.get(MaterialID::BatCat));
    entity.addComponent<cro::Transform>().setScale({ 0.002f, 0.002f, 0.002f });
    entity.getComponent<cro::Transform>().setRotation({ -cro::Util::Const::PI / 2.f, cro::Util::Const::PI / 2.f, 0.f });
    entity.addComponent<cro::Skeleton>() = batCatSkeleton;

    //starting room
    entity = m_scene.createEntity();
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::RoomTwo), m_materialResource.get(MaterialID::RoomTwo));
    entity.addComponent<cro::Transform>().move({ 0.f, 0.5f, 0.5f });

    //3D camera
    auto ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 0.f, 0.5f, 2.3f });
    ent.addComponent<cro::Camera>();
    ent.addComponent<cro::CommandTarget>().ID = CommandID::Camera;
    m_scene.setActiveCamera(ent);

    //2D camera
    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>();
    auto& cam2D = ent.addComponent<cro::Camera>();
    cam2D.projection = glm::ortho(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -0.1f, 100.f);
    m_overlayScene.setActiveCamera(ent);

    ent = m_overlayScene.createEntity();
    auto& font = m_fontResource.get(0);
    font.loadFromFile("assets/VeraMono.ttf");
    auto& text = ent.addComponent<cro::Text>(font);
    text.setColour(cro::Colour::Blue());
    //text.setString("WASD/MouseWheel to control view\nCursor keys to regenerate");
    text.setString("WASD to move");
    ent.addComponent<cro::Transform>().setPosition({ 50.f, 1060.f, 0.f });
}

void GameState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam3D = m_scene.getActiveCamera().getComponent<cro::Camera>();
    cam3D.projection = glm::perspective(0.6f, 16.f / 9.f, 0.1f, 100.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    auto& cam2D = m_overlayScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}

void GameState::buildRow(std::size_t floor)
{
    addStairs(floor);

    cro::int32 position = 1;
    while (position < rowWidth)
    {
        cro::int32 step = (1 + (cro::Util::Random::value(2, 3) % 2));
        if (step == 1)
        {
            addSingle(floor, position);
        }
        else
        {
            addDouble(floor, position);
        }
        position += step;
    }

    cro::int32 endSize = (position > rowWidth) ? 1 : 2;
    addEnd(floor, position, endSize);

    position = 0;
    while (position > -(rowWidth - 1))
    {
        cro::int32 step = (1 + (cro::Util::Random::value(2, 3) % 2));
        position -= step;        
        if (step == 1)
        {
            addSingle(floor, position);
        }
        else
        {
            addDouble(floor, position);
        }
    }

    endSize = (position <= -rowWidth) ? 1 : 2;
    addEnd(floor, position - endSize, endSize);
}

void GameState::addStairs(std::size_t floor)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.5f, static_cast<float>(floor), 0.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(floor == 0 ? MaterialID::Blue : MaterialID::Red));
    entity.addComponent<cro::CommandTarget>().ID = (1 << (rowCount * rowWidth));
}

void GameState::addSingle(std::size_t floor, cro::int32 position)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ static_cast<float>(position) + 0.5f, static_cast<float>(floor), 0.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::RoomOne), m_materialResource.get(MaterialID::RoomOne));
    entity.addComponent<cro::CommandTarget>().ID = (1 << (rowCount * rowWidth));
}

void GameState::addDouble(std::size_t floor, cro::int32 position)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ static_cast<float>(position) + 1.f, static_cast<float>(floor), 0.f });
    //entity.getComponent<cro::Transform>().setScale({ 2.f, 1.f, 1.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::RoomTwo), m_materialResource.get(MaterialID::RoomTwo));
    entity.addComponent<cro::CommandTarget>().ID = (1 << (rowCount * rowWidth));
}

void GameState::addEnd(std::size_t floor, cro::int32 position, std::size_t width)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ static_cast<float>(position) + (static_cast<float>(width) / 2.f), static_cast<float>(floor), 0.f });

    if (width > 1)
    {
        entity.getComponent<cro::Transform>().setScale({ static_cast<float>(width), 1.f, 1.f });
    }

    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(MaterialID::Brown));
    entity.addComponent<cro::CommandTarget>().ID = (1 << (rowCount * rowWidth));
}