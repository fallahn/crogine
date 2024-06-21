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
#include <crogine/detail/OpenGL.hpp>

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

    constexpr glm::vec3 Gravity(0.f, -9.8f, 0.f);
    constexpr float Dampening = 0.33f;

    const std::array<std::string, 3u> LevelStrings =
    {
        std::string("Novice"),
        std::string("Expert"),
        std::string("Pro"),
    };
}



ArcState::ArcState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_clubID        (0),
    m_clubLevel     (0),
    m_zoom          (2.5f)
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
            ImGui::SetNextWindowSize({ 316.f, 200.f });
            if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
            {
                //select club ID
                if (ImGui::InputInt("Club:", &m_clubID))
                {
                    m_clubID = (m_clubID + ClubID::Count) % ClubID::Count;
                    plotArc();
                }
                ImGui::SameLine();
                ImGui::Text("%s", Clubs[m_clubID].name.c_str());

                //select club novice/expert/pro
                if (ImGui::InputInt("Level:", &m_clubLevel))
                {
                    m_clubLevel = (m_clubLevel + 3) % 3;
                    plotArc();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LevelStrings[m_clubLevel].c_str());

                static constexpr float ColourSize = 12.f;
                ImGui::ColorButton("##0", { 0.f, 1.f, 0.f, 0.f }, 0, { ColourSize, ColourSize }); ImGui::SameLine();
                ImGui::Text("Default Distance: %3.2f, Target: %3.2f", m_distances[ShotType::Default], ClubStats[m_clubID].stats[m_clubLevel].target);
                ImGui::ColorButton("##1", { 1.f, 0.f, 0.f, 0.f }, 0, { ColourSize, ColourSize }); ImGui::SameLine();
                ImGui::Text("Punch Distance:   %3.2f, Target: %3.2f", m_distances[ShotType::Punch], ClubStats[m_clubID].stats[m_clubLevel].target);
                ImGui::ColorButton("##2", { 0.f, 0.f, 1.f, 0.f }, 0, { ColourSize, ColourSize }); ImGui::SameLine();
                ImGui::Text("Flop Distance:    %3.2f, Target: %3.2f", m_distances[ShotType::Flop], ClubStats[m_clubID].stats[m_clubLevel].target);

                //angle for flop
                //power multiplier for flop

                //angle for punch
                //power multiplier for punch


                //plot button to calc arc
                if (ImGui::Button("Refresh"))
                {
                    plotArc();
                }


                ImGui::Separator();

                //zoom
                if (ImGui::SliderFloat("Zoom", &m_zoom, 0.5f, 3.f))
                {
                    m_zoom = std::clamp(m_zoom, 0.5f, 3.f);
                    m_plotEntity.getComponent<cro::Transform>().setScale(glm::vec2(m_zoom));
                }
            }
            ImGui::End();
        });


    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 10.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(m_zoom));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    m_plotEntity = entity;
    plotArc();

    auto resize = [&](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

void ArcState::plotArc()
{
    //green default arc
    //red punch
    //blue flop

    const std::array<cro::Colour, ShotType::Count> Colours =
    {
        cro::Colour::Green, cro::Colour::Blue, cro::Colour::Red
    };

    std::vector<cro::Vertex2D> verts;

    const auto genVerts = [&](float angle, float power, std::int32_t shotType)
        {
            glm::vec3 impulse = glm::vec3(1.f, 0.f, 0.f) * glm::rotate(cro::Transform::QUAT_IDENTITY, -angle, cro::Transform::Z_AXIS);
            impulse *= power;

            verts.emplace_back(glm::vec2(0.f), cro::Colour::Transparent);
            verts.emplace_back(glm::vec2(0.f), Colours[shotType]);
            glm::vec3 pos(0.f);

            static constexpr float Step = 1.f / 60.f; //TODO make this variable?
            static constexpr std::int32_t MaxStep = 700;
            std::int32_t stepCount = 0;

            do
            {
                pos += impulse * Step;
                impulse += Gravity * Step;

                if (pos.y < 0.f)
                {
                    pos.y = 0.f;
                    impulse = glm::reflect(impulse, cro::Transform::Y_AXIS);
                    impulse *= Dampening;
                }

                verts.emplace_back(glm::vec2(pos.x, pos.y), Colours[shotType]);
                stepCount++;

            } while ((glm::length2(impulse) > 0.01f) && stepCount < MaxStep);

            verts.emplace_back(pos, cro::Colour::Transparent);
            m_distances[shotType] = pos.x;
        };

    genVerts(Clubs[m_clubID].angle, ClubStats[m_clubID].stats[m_clubLevel].power, ShotType::Default);
    genVerts(1.f, 50.f, ShotType::Punch);
    genVerts(1.2f, 30.f, ShotType::Flop);

    m_plotEntity.getComponent<cro::Drawable2D>().setVertexData(verts);
}