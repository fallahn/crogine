//Auto-generated source file for Scratchpad Stub 08/01/2024, 12:14:56

#include "EndlessDrivingState.hpp"

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
    //TODO move these to some Const header
    constexpr glm::uvec2 PlayerSize = glm::uvec2(160);
    constexpr glm::uvec2 RenderSize = glm::uvec2(640, 448);
    constexpr glm::vec2 RenderSizeFloat = glm::vec2(RenderSize);

    //TODO encapsulate in relative classes
    
    //rendering
    float roadWidth = 2000.f; //road is +/- this
    constexpr float segmentLength = 200.f;
    constexpr std::int32_t rumbleLength = 3; //number of segs per strip
    constexpr std::int32_t drawDistance = 100; //max number of road segments to process and draw
    float trackLength = 100000.f; //total length before looping
    float lanes = 3.f; //might be superfluous
    float fov = 60.f;
    float cameraHeight = 1000.f;
    float cameraDepth = 0.f;
    float fogDensity = 5.f;

    //player specific
    float playerX = 0.f; //+/- 1 from X centre
    float playerZ = 0.f; //rel distance from camera
    float camZ = 0.f; //add player z to get absolute player pos
    float speed = 0.f;
    //TODO constify these - current non-const so we can play with imgui
    constexpr float MaxSpeed = segmentLength * 60.f; //60 is our frame time
    float acceleration = MaxSpeed / 5.f;
    float braking = -MaxSpeed;
    float deceleration = -acceleration;
    float offroadDeceleration = -MaxSpeed / 2.f;
    float offroadMaxSpeed = MaxSpeed / 4.f;

    struct InputFlags final
    {
        enum
        {
            Up = 0x1,
            Down = 0x2,
            Left = 0x4,
            Right = 0x8
        };
        std::uint16_t flags = 0;
        std::uint16_t prevFlags = 0;
    }inputFlags;

    //TODO move this to player class
    void updatePlayer(float dt, cro::Entity entity)
    {
        //I'm sure there are better ways to wrap around but meh
        camZ += speed * dt;
        if (camZ > trackLength)
        {
            camZ -= trackLength;
        }

        const float dx = dt * 2.f * (speed / MaxSpeed);
        if (inputFlags.flags & InputFlags::Left)
        {
            playerX -= dx;
        }

        if (inputFlags.flags & InputFlags::Right)
        {
            playerX += dx;
        }

        
        if ((inputFlags.flags & (InputFlags::Up | InputFlags::Down)) == 0)
        {
            //free wheel decel
            speed += deceleration * dt;
        }
        else
        {
            if (inputFlags.flags & InputFlags::Up)
            {
                speed += acceleration * dt;
            }
            if (inputFlags.flags & InputFlags::Down)
            {
                speed += braking * dt;
            }
        }

        //+/-1 is off road
        if ((playerX < -1 || playerX > 1)
            && (speed > offroadMaxSpeed))
        {
            //slow down
            speed += offroadDeceleration * dt;
        }

        playerX = std::clamp(playerX, -2.f, 2.f);
        speed = std::clamp(speed, 0.f, MaxSpeed);

        //TODO only update on FOV change
        //TODO set fov in rads so we don't have to keep converting
        cameraDepth = 1.f / std::tan((fov / 2.f) * cro::Util::Const::degToRad);
        playerZ = cameraHeight * cameraDepth;

        //TODO rotate player model with steering

        //update sprite ent position (playerX is +/- 2)
        const float x = ((RenderSizeFloat.x / 4.f) * playerX) + (RenderSizeFloat.x / 2.f);
        const float scale = cameraDepth / playerZ;
        entity.getComponent<cro::Transform>().setPosition(glm::vec2(x, 0.f));
        //entity.getComponent<cro::Transform>().setScale(glm::vec2(scale)); //TODO fix this
    }



    //TODO move this to own file
    struct Point final
    {
        glm::vec3 world = glm::vec3(0.f);
        glm::vec3 camera = glm::vec3(0.f);
        glm::vec4 screen = glm::vec4(0.f);
        float scale = 1.f;
    };
    void project(Point& p, glm::vec3 camera, float cameraDepth)
    {
        p.camera = p.world - camera;
        p.scale = cameraDepth / p.camera.z;

        const auto screenCentre = RenderSizeFloat / 2.f;
        p.screen.x = std::round(screenCentre.x + (p.scale * p.camera.x * screenCentre.x));
        p.screen.y = std::round(screenCentre.y + (p.scale * p.camera.y * screenCentre.y));
        p.screen.w = std::round(p.scale * roadWidth * screenCentre.x);
    }

    struct Segment final
    {
        Point p0;
        Point p1;
        cro::Colour colour;
    };
    std::vector<Segment> road;

    void createRoad()
    {
        static constexpr std::int32_t SegmentCount = 250;

        for (auto i = 0; i < SegmentCount; ++i)
        {
            auto& seg = road.emplace_back();
            seg.p0.world.z = i * segmentLength;
            seg.p1.world.z = (i + 1) * segmentLength;

            seg.colour = ((i / rumbleLength) % 2) ? cro::Colour::White : cro::Colour::LightGrey;
        }

        trackLength = SegmentCount * segmentLength;
    }

    void updateRoad(float dt, cro::Entity entity)
    {
        const std::int32_t baseSeg = std::int32_t(camZ / segmentLength) % road.size();
        float maxY = RenderSizeFloat.y;

        for (auto i = 0; i < drawDistance; ++i)
        {
            //TODO maintain these camera positions somewhere
            auto& seg = road[(baseSeg + i) % road.size()];
            project(seg.p0, { playerX * roadWidth, cameraHeight, camZ }, cameraDepth);
            project(seg.p1, { playerX * roadWidth, cameraHeight, camZ }, cameraDepth);

            if (seg.p0.camera.z < cameraDepth //behind us
                || seg.p1.screen.y > maxY) //out of view
            {
                continue;
            }

            //TODO insert into vertex array

            maxY = seg.p1.screen.y;
        }
    }
}

EndlessDrivingState::EndlessDrivingState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_playerScene   (context.appInstance.getMessageBus()),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createPlayer();
        createScene();
        createUI();
    });

    registerWindow([&]() 
        {
            if (ImGui::Begin("Player"))
            {
                static const auto Red = ImVec4(1.f, 0.f, 0.f, 1.f);
                static const auto Green = ImVec4(0.f, 1.f, 0.f, 1.f);
                if (inputFlags.flags & InputFlags::Left)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Green);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Red);
                }
                ImGui::Text("Left");
                ImGui::PopStyleColor();

                if (inputFlags.flags & InputFlags::Right)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Green);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Red);
                }
                ImGui::Text("Right");
                ImGui::PopStyleColor();

                ImGui::Text("Speed %3.3f", speed);
                ImGui::Text("PlayerX %3.3f", playerX);
            }
            ImGui::End();
        });
}

//public
bool EndlessDrivingState::handleEvent(const cro::Event& evt)
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

            //TODO move this to player/input and read keybinds
        case SDLK_w:
            inputFlags.flags |= InputFlags::Up;
            break;
        case SDLK_s:
            inputFlags.flags |= InputFlags::Down;
            break;
        case SDLK_a:
            inputFlags.flags |= InputFlags::Left;
            break;
        case SDLK_d:
            inputFlags.flags |= InputFlags::Right;
            break;
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
            //TODO move this to player/input and read keybinds
        case SDLK_w:
            inputFlags.flags &= ~InputFlags::Up;
            break;
        case SDLK_s:
            inputFlags.flags &= ~InputFlags::Down;
            break;
        case SDLK_a:
            inputFlags.flags &= ~InputFlags::Left;
            break;
        case SDLK_d:
            inputFlags.flags &= ~InputFlags::Right;
            break;
        }
    }

    m_playerScene.forwardEvent(evt);
    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void EndlessDrivingState::handleMessage(const cro::Message& msg)
{
    m_playerScene.forwardMessage(msg);
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool EndlessDrivingState::simulate(float dt)
{
    updateRoad(dt, m_roadEntity);
    updatePlayer(dt, m_playerEntity);

    m_playerScene.simulate(dt);
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void EndlessDrivingState::render()
{
    m_playerTexture.clear(cro::Colour::Transparent);
    m_playerScene.render();
    m_playerTexture.display();

    m_gameTexture.clear(cro::Colour::Magenta);
    m_gameScene.render();
    m_gameTexture.display();

    m_uiScene.render();
}

//private
void EndlessDrivingState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_playerScene.addSystem<cro::CameraSystem>(mb);
    m_playerScene.addSystem<cro::ModelRenderer>(mb);

    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SpriteSystem2D>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::RenderSystem2D>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void EndlessDrivingState::loadAssets()
{
}

void EndlessDrivingState::createPlayer()
{
    m_playerTexture.create(PlayerSize.x, PlayerSize.y);

    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/cars/cart.cmt"))
    {
        auto entity = m_playerScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
    }

    auto resize = [](cro::Camera& cam)
        {
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setPerspective(cro::Util::Const::degToRad * 54.f, 1.f, 0.1f, 10.f);
        };

    auto& cam = m_playerScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_playerScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ -2.6f, 1.5f, 0.f });
    m_playerScene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -cro::Util::Const::PI / 2.f);
    m_playerScene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.28f);

    m_playerScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
}

void EndlessDrivingState::createScene()
{
    //background
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -9.5f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_resources.textures.get("assets/cars/sky.png"));

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -9.4f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_resources.textures.get("assets/cars/hills.png"));

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -9.3f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_resources.textures.get("assets/cars/trees.png"));


    //player
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(320.f, 0.f, -0.1f));
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(static_cast<float>(PlayerSize.x / 2u), 0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_playerTexture.getTexture());
    m_playerEntity = entity;

    //road
    createRoad();
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -8.f));
    entity.addComponent<cro::Drawable2D>();
    m_roadEntity = entity;

    auto resize = [](cro::Camera& cam)
    {
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setOrthographic(0.f, static_cast<float>(RenderSize.x), 0.f, static_cast<float>(RenderSize.y), -0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

void EndlessDrivingState::createUI()
{
    m_gameTexture.create(RenderSize.x, RenderSize.y, false);

    cro::Entity entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin(glm::vec2(RenderSize / 2u));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_gameTexture.getTexture());
    m_gameEntity = entity;

    //TODO add border overlay (recycle GvG?)

    auto resize = 
        [&](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -1.f, 10.f);

        const float scale = std::max(1.f, std::floor(size.y / RenderSize.y));
        m_gameEntity.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        m_gameEntity.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -0.1f));
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}