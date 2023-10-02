//Auto-generated source file for Scratchpad Stub 02/10/2023, 12:05:10

#include "BounceState.hpp"

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

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/detail/OpenGL.hpp>

namespace
{
    constexpr glm::vec2 WindowSize(320.f, 320.f);
    constexpr float Gravity = -9.98f;
    constexpr float BallRadius = 20.f;



    struct BallData final
    {
        glm::vec2 velocity = glm::vec2(0.f);
        float mass = 1.f;
        float health = 40.f;// 100.f;
        float lifetime = 5.f;

        const float radius = BallRadius;
        const float restitution = 0.9f;

        cro::Scene& parentScene;
        explicit BallData(cro::Scene& scene, float rad = BallRadius, float rest = 0.9f)
            : parentScene(scene), radius(rad), restitution(rest) {}

        std::function<cro::Entity(float, float)> createBall;
    };

    struct BallPhysics final
    {
        void operator()(cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<BallData>();

            //apply gravity
            data.velocity.y += Gravity;

            //do motion
            auto& tx = e.getComponent<cro::Transform>();
            tx.move(data.velocity * dt);

            glm::vec2 windowMove(0.f);

            //do collision
            auto pos = tx.getPosition();
            if (pos.x < data.radius)
            {
                tx.setPosition({ data.radius, pos.y });
                data.velocity = glm::reflect(data.velocity, glm::vec2(1.f, 0.f));
                windowMove.x = pos.x - data.radius;
            }
            else if (pos.x > (WindowSize.x - data.radius))
            {
                tx.setPosition({ WindowSize.x - data.radius, pos.y });
                data.velocity = glm::reflect(data.velocity, glm::vec2(-1.f, 0.f));
                windowMove.x = pos.x - (WindowSize.x - data.radius);
            }

            if (pos.y < data.radius)
            {
                tx.setPosition({ pos.x, data.radius });
                data.velocity = glm::reflect(data.velocity, glm::vec2(0.f, 1.f));
                windowMove.y = data.radius - pos.y; //remember window coords are inverted Y
            }
            else if (pos.y > (WindowSize.y - data.radius))
            {
                tx.setPosition({ pos.x, WindowSize.y - data.radius });
                data.velocity = glm::reflect(data.velocity, glm::vec2(0.f, -1.f));
                windowMove.y = (WindowSize.y - data.radius) - pos.y;
            }
            windowMove *= 2.f;

            if (auto l2 = glm::length2(windowMove); l2 != 0)
            {
                data.velocity *= data.restitution;

                const auto len = std::sqrt(l2);
                data.health -= len * 0.3f;
                windowMove *= std::max(1.f, len * 0.1f) * data.mass;
            }

            auto currPos = cro::App::getWindow().getPosition();
            currPos.x += static_cast<std::int32_t>(std::round(windowMove.x));
            currPos.y += static_cast<std::int32_t>(std::round(windowMove.y));
            cro::App::getWindow().setPosition(currPos.x, currPos.y);


            //check health
            if (data.health < 0)
            {
                pos = tx.getPosition();
                auto vel = data.velocity;

                //mini balls
                if (data.mass != 0)
                {
                    for (auto i = 0u; i < 5; ++i)
                    {
                        auto b = data.createBall(BallRadius / 5.f, 0.5f);
                        b.getComponent<cro::Transform>().setPosition(pos);
                        b.getComponent<cro::Callback>().getUserData<BallData>().velocity = vel + glm::vec2(0.f, (i - 2.f) * 20.f);
                        b.getComponent<cro::Callback>().getUserData<BallData>().mass = 0.f;
                    }
                }
                e.getComponent<cro::Callback>().active = false;
                data.parentScene.destroyEntity(e);
            }

            data.lifetime -= dt * (1.f - data.mass); //this is a hack which won't work if mass > 1
            if (data.lifetime < 0)
            {
                e.getComponent<cro::Callback>().active = false;
                data.parentScene.destroyEntity(e);
            }
        }
    };

    cro::Entity ball;
    std::vector<cro::Entity> balls;
}

BounceState::BounceState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.setSize({ 320, 320 });
    
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool BounceState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    auto buttonPress = [&]()
    {
        if (!ball.isValid())
        {
            ball = createBall(BallRadius);
        }
        else
        {
            //create a random impulse
            glm::vec2 impulse(0.f);
            impulse.x = cro::Util::Random::value(0, 1) == 0 ? -1.f : 1.f;
            impulse.x *= cro::Util::Random::value(0.1f, 1.f);

            impulse.y = /*cro::Util::Random::value(0, 1) == 0 ? -1.f :*/ 1.f;
            impulse.y *= cro::Util::Random::value(0.1f, 1.f);

            impulse = glm::normalize(impulse) * static_cast<float>(cro::Util::Random::value(500, 1500));
            ball.getComponent<cro::Callback>().getUserData<BallData>().velocity += impulse;
        }
    };

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            cro::App::quit();
            break;
        case SDLK_SPACE:
            buttonPress();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonA:
            buttonPress();
            break;
        }
    }
    else if (evt.type == SDL_WINDOWEVENT)
    {
        if (evt.window.event == SDL_WINDOWEVENT_MOVED)
        {
            SDL_Log("Window %d moved to %d,%d",
                evt.window.windowID, evt.window.data1,
                evt.window.data2);
        }
        else if(evt.window.event == SDL_WINDOWEVENT_EXPOSED)
        {
            //interesting... setting the window manually
            //raises this event... 
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void BounceState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool BounceState::simulate(float dt)
{
    balls.erase(std::remove_if(balls.begin(), balls.end(), 
        [](const cro::Entity e)
        {
            return !e.isValid();
        }), balls.end());


    //inter-ball collision. I know, but this is a scratch pad so it can be as messy as I like :P
    for (auto ball : balls)
    {
        const float radius = ball.getComponent<cro::Callback>().getUserData<BallData>().radius;
        auto& tx = ball.getComponent<cro::Transform>();

        for (auto other : balls)
        {
            if (other != ball)
            {
                const float otherRadius = other.getComponent<cro::Callback>().getUserData<BallData>().radius + radius;
                auto& otherTx = other.getComponent<cro::Transform>();

                auto dir = tx.getPosition() - otherTx.getPosition();
                const float r2 = otherRadius * otherRadius;
                const float l2 = glm::length2(dir);

                if (l2 < r2
                    && l2 != 0)
                {
                    const float len = std::sqrt(l2);
                    const float overlap = otherRadius - len;
                    dir /= len;
                    tx.move(dir * (overlap / 2.f));

                    auto vel = ball.getComponent<cro::Callback>().getUserData<BallData>().velocity;
                    vel = glm::reflect(vel, glm::vec2(-dir));
                    //vel *= 0.8f;// ball.getComponent<cro::Callback>().getUserData<BallData>().restitution;
                    ball.getComponent<cro::Callback>().getUserData<BallData>().velocity = vel;
                }
            }
        }
    }

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void BounceState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void BounceState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);


    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void BounceState::loadAssets()
{
}

void BounceState::createScene()
{
    //TODO we're not using this right now
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

void BounceState::createUI()
{
    auto resize = [](cro::Camera& cam)
    {
        cro::App::getWindow().setSize({320, 320});

        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

cro::Entity BounceState::createBall(float radius, float restitution)
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ WindowSize / 2.f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);


    std::vector<cro::Vertex2D> vertices;
    constexpr float PointCount = 16.f;
    constexpr float Segment = cro::Util::Const::TAU / PointCount;
    for (auto i = 0.f; i < PointCount; ++i)
    {
        const float x = std::cos(i * Segment);
        const float y = std::sin(i * Segment);

        vertices.emplace_back(glm::vec2(x, y) * radius, cro::Colour::Magenta);
    }
    vertices.push_back(vertices.front());
    entity.getComponent<cro::Drawable2D>().setVertexData(vertices);

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<BallData>(m_uiScene, radius, restitution);
    entity.getComponent<cro::Callback>().getUserData<BallData>().createBall = std::bind(&BounceState::createBall, this, std::placeholders::_1, std::placeholders::_2); 
    entity.getComponent<cro::Callback>().function = BallPhysics();

    balls.push_back(entity);

    return entity;
}