//Auto-generated source file for Scratchpad Stub 20/06/2024, 15:19:50

#include "ArcState.hpp"

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

namespace
{
    struct ClubID final
    {
        enum
        {
            Driver, ThreeWood, FiveWood,
            FourIron, FiveIron, SixIron,
            SevenIron, EightIron, NineIron,
            PitchWedge, GapWedge, SandWedge,
            Putter,

            Count
        };
    };

    //---------------------------

    struct Stat final
    {
        constexpr Stat() = default;
        constexpr Stat(float p, float t) : power(p), target(t) {}
        float power = 0.f; //impulse strength
        float target = 0.f; //distance target
    };

    struct ClubStat final
    {
        constexpr ClubStat(Stat a, Stat b, Stat c)
            : stats() {
            stats = { a, b, c };
        }
        std::array<Stat, 3> stats = {};
    };

    constexpr std::array<ClubStat, ClubID::Count> ClubStats =
    {
        ClubStat({44.f,   220.f},{46.18f, 240.f},{48.2f,  260.f}), //123
        ClubStat({39.6f,  180.f},{42.08f, 200.f},{44.24f, 220.f}), //123
        ClubStat({36.3f,  150.f},{37.82f, 160.f},{39.66f, 180.f}), //123

        ClubStat({35.44f, 140.f},{36.49f, 150.f},{37.69f, 160.f}), //123
        ClubStat({34.16f, 130.f},{35.44f, 140.f},{36.49f, 150.f}), //123
        ClubStat({32.85f, 120.f},{34.16f, 130.f},{35.44f, 140.f}), //123
        ClubStat({31.33f, 110.f},{32.85f, 120.f},{34.16f, 130.f}), //123
        ClubStat({29.91f, 100.f},{31.33f, 110.f},{32.85f, 120.f}), //123
        ClubStat({28.4f,  90.f}, {29.91f, 100.f},{31.33f, 110.f}), //123


        ClubStat({25.2f, 70.f}, {25.98f, 75.f}, {27.46f, 80.f}),
        ClubStat({17.4f, 30.f}, {18.53f, 32.f}, {19.14f, 35.f}),
        //this won't increase in range
        ClubStat({10.3f, 10.f}, {10.3f, 10.f}, {10.3f, 10.f}),
        ClubStat({9.11f, 10.f}, {9.11f, 10.f}, {9.11f, 10.f}) //except this which is dynamic
    };

    //--------------------------

    struct Club final
    {
        std::int32_t id = 0;
        std::string name;
        float angle = 0.f;

        Club(std::int32_t i, const std::string& n, float a)
            : id(i), name(n),angle(a* cro::Util::Const::degToRad) {}
    };


    static const std::array<Club, ClubID::Count> Clubs =
    {
        Club(ClubID::Driver,    "Driver ", 45.f),
        Club(ClubID::ThreeWood, "3 Wood ", 45.f),
        Club(ClubID::FiveWood,  "5 Wood ", 45.f),


        Club(ClubID::FourIron,  "4 Iron ", 40.f),
        Club(ClubID::FiveIron,  "5 Iron ", 40.f),
        Club(ClubID::SixIron,   "6 Iron ", 40.f),
        Club(ClubID::SevenIron, "7 Iron ", 40.f),
        Club(ClubID::EightIron, "8 Iron ", 40.f),
        Club(ClubID::NineIron,  "9 Iron ", 40.f),


        Club(ClubID::PitchWedge, "Pitch Wedge ", 52.f),
        Club(ClubID::GapWedge,   "Gap Wedge ",   60.f),
        Club(ClubID::SandWedge,  "Sand Wedge ",  60.f),
        Club(ClubID::Putter,     "Putter ",      0.f)
    };

    constexpr glm::vec2 Gravity(0.f, -9.8f);

    /*
    Impulse = vec2(1.f, 0.f) * rotation(club.angle) * clubStats[club].stat[level].power;
    
    vel = impulse
    while(pos.y > 0)
    {
        pos += vel * dt
        vel += Gravity * dt
    }

    */
}



ArcState::ArcState(cro::StateStack& stack, cro::State::Context context)
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
bool ArcState::handleEvent(const cro::Event& evt)
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
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void ArcState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool ArcState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void ArcState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void ArcState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void ArcState::loadAssets()
{
}

void ArcState::createScene()
{
    //TODO remove/disable game scene

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

void ArcState::createUI()
{
    registerWindow([&]() 
        {
            //select club ID
            //select club novice/expert/pro

            //plot button to calc arc

            //angle for flop
            //power multiplier for flop

            //angle for punch
            //power multiplier for punch
    

            //default to 2px per metre and control to scale view
            //OR auto scale view to fit result

        });

    //entity @ 0,0 with line strip vert array
    //green default arc
    //red punch
    //blue flop



    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}