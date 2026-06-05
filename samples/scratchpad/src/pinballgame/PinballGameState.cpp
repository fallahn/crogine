//Auto-generated source file for Scratchpad Stub 04/06/2026, 12:33:34

#include "PinballGameState.hpp"
#include "PinballSystem.hpp"
#include "PinballConsts.hpp"

#include <box2d/box2d.h>

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/Shape2D.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/util/Constants.hpp>

namespace
{

}

//later on I'm going to move this to the game scene
//so this is a pre-emtive hack
#define SCENE m_uiScene

PinballGameState::PinballGameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool PinballGameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            requestStackClear();
            requestStackPush(0);
            break;
        case SDLK_SPACE:
            spawnBall();
            break;
        case SDLK_LEFT:
        {
            auto j = SCENE.getSystem<PinballSystem>()->testJoint;
            b2MotorJoint_SetAngularVelocity(j, 10.f);
            b2Joint_WakeBodies(j);
            break;
        }
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_LEFT:
            b2MotorJoint_SetAngularVelocity(SCENE.getSystem<PinballSystem>()->testJoint, 0.f);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void PinballGameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool PinballGameState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void PinballGameState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void PinballGameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    //TODO if we want to render 3D move the pinball
    //system to the Game scene
    SCENE.addSystem<PinballSystem>(mb)->createTable();

    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);


    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void PinballGameState::loadAssets()
{
}

void PinballGameState::createScene()
{
    //table border
    auto entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>().setOrigin(TableSize / 2.f);
    entity.addComponent<cro::Drawable2D>().setVertexData(cro::Shape::rectangle(TableSize, cro::Colour::Green));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);


    const auto halfBox = 
        [](glm::vec2 halfSize)
        {
            std::vector<cro::Vertex2D> ret;
            ret.emplace_back(glm::vec2(-halfSize.x, halfSize.y), cro::Colour::Red);
            ret.emplace_back(halfSize, cro::Colour::Red);
            ret.emplace_back(glm::vec2(halfSize.x, -halfSize.y), cro::Colour::Red);
            ret.emplace_back(-halfSize, cro::Colour::Red);

            ret.push_back(ret.front());
            return ret;
        };

    const auto arcSeg =
        [this, &halfBox](glm::vec2 pos, float rotation)
        {
            auto entity = SCENE.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos);
            entity.getComponent<cro::Transform>().setRotation(rotation);
            entity.addComponent<cro::Drawable2D>().setVertexData(halfBox(ArcSegmentSize));
            entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
        };

    ////walls
    //for (const auto& [pos, size] : WallPosSize)
    //{
    //    entity = SCENE.createEntity();
    //    entity.addComponent<cro::Transform>().setPosition(pos);
    //    entity.addComponent<cro::Drawable2D>().setVertexData(halfBox(size));
    //    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    //}


    //table arc
    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    
    const auto arc = getArc();
    std::vector<cro::Vertex2D> verts;
    for (const auto& p : arc)
    {
        verts.emplace_back(p, cro::Colour::Green);
    }

    entity.getComponent<cro::Drawable2D>().setVertexData(verts);


    //channel wall
    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>().setPosition(ChannelWallPos);
    entity.addComponent<cro::Drawable2D>().setVertexData(halfBox(ChannelWallSize));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);


    //bottom funnel
    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>().setPosition(FunnelLeftPos);
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.getComponent<cro::Drawable2D>().setVertexData({
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::Red),
        cro::Vertex2D(glm::vec2(FunnelSize.x, 0.f), cro::Colour::Red),
        cro::Vertex2D(glm::vec2(0.f, FunnelSize.y), cro::Colour::Red),
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::Red),
        });

    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>().setPosition(FunnelRightPos);
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.getComponent<cro::Drawable2D>().setVertexData({
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::Red),
        cro::Vertex2D(glm::vec2(FunnelSize.x, 0.f), cro::Colour::Red),
        cro::Vertex2D(FunnelSize, cro::Colour::Red),
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::Red),
        });


    //ramp on the left
    verts.clear();
    for (const auto& p : RampPoints)
    {
        verts.emplace_back(p, cro::Colour::Red);
    }
    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);



    //bumpers
    verts.clear();
    for (const auto p : BumperPoints)
    {
        verts.emplace_back(p, cro::Colour::Blue);
    }
    verts.push_back(verts.front());

    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);

    std::reverse(verts.begin(), verts.end());
    for (auto& p : verts)
    {
        p.position.x *= -1.f;
        p.position.x -= ((TableSize.x / 2.f) - PlayAreaCentre) * 2.f;
    }
    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);



    //flipper chute
    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>().setPosition(FlipperChutePosLarge);
    entity.getComponent<cro::Transform>().setRotation(FlipperChuteLargeRotation);
    entity.addComponent<cro::Drawable2D>().setVertexData(halfBox(FlipperChuteSizeLarge / 2.f));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);


    auto flippedPos = FlipperChutePosLarge;
    flippedPos.x *= -1.f;
    flippedPos.x -= ((TableSize.x / 2.f) - PlayAreaCentre) * 2.f;

    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>().setPosition(flippedPos);
    entity.getComponent<cro::Transform>().setRotation(-FlipperChuteLargeRotation);
    entity.addComponent<cro::Drawable2D>().setVertexData(halfBox(FlipperChuteSizeLarge / 2.f));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);




    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>().setPosition(FlipperChutePosSmall);
    entity.addComponent<cro::Drawable2D>().setVertexData(halfBox(FlipperChuteSizeSmall / 2.f));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);


    flippedPos = FlipperChutePosSmall;
    flippedPos.x *= -1.f;
    flippedPos.x -= ((TableSize.x / 2.f) - PlayAreaCentre) * 2.f;

    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>().setPosition(flippedPos);
    entity.addComponent<cro::Drawable2D>().setVertexData(halfBox(FlipperChuteSizeSmall / 2.f));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);




    //flippers
    entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(halfBox({ 0.02f, 0.05f }));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [this](cro::Entity e, float)
        {
            const auto body = SCENE.getSystem<PinballSystem>()->testBody;
            const b2Rot rotation = b2Body_GetRotation(body);
            const auto rads = b2Rot_GetAngle(rotation);
            e.getComponent<cro::Transform>().setRotation(rads);

            const b2Vec2 position = b2Body_GetPosition(body);
            e.getComponent<cro::Transform>().setPosition({ position.x, position.y });
        };


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.8f, 2.f });
}

void PinballGameState::createUI()
{

    //camera has 0,0 at centre of screen
    //TODO change this when we move to 3D rendering
    auto resize = [](cro::Camera& cam)
    {
        const float height = TableSize.y + 0.1f;
        glm::vec2 size(cro::App::getWindow().getSize());
        size.x *= height / size.y; //scale vertical height to that of the table
        size.y = height;

        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(-size.x / 2.f, size.x / 2.f, -size.y / 2.f, size.y / 2.f, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_uiScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ TableSize.x / 2.f, 0.f, 0.f });
}

void PinballGameState::spawnBall()
{
    auto entity = SCENE.createEntity();
    entity.addComponent<cro::Transform>().setPosition(PinballSpawn);
    entity.addComponent<cro::Drawable2D>().setVertexData(cro::Shape::circle(PinballRadius, cro::Colour::Magenta));
    entity.getComponent<cro::Drawable2D>().getVertexData().emplace_back(glm::vec2(0.f), cro::Colour::Magenta);
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.addComponent<Pinball>();
}