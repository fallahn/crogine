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
#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtx/euler_angles.hpp>

namespace
{
    constexpr glm::uvec2 RenderSize = glm::uvec2(320, 224);
    constexpr glm::vec2 RenderSizeFloat = glm::vec2(RenderSize);
    constexpr float PlayerWidth = 74.f;
    constexpr float PlayerHeight = 84.f;
    constexpr cro::FloatRect PlayerBounds((RenderSizeFloat.x - PlayerWidth) / 2.f, 0.f, PlayerWidth, PlayerHeight);

    constexpr float fogDensity = 5.f;
    float expFog(float distance, float density)
    {
        float fogAmount = 1.f / (std::pow(cro::Util::Const::E, (distance * distance * density)));
        fogAmount = (0.5f * (1.f - fogAmount));

        fogAmount = std::round(fogAmount * 25.f);
        fogAmount /= 25.f;

        return fogAmount;
    }
    const cro::Colour FogColour = CD32::Colours[CD32::GreenDark];

    struct Debug final
    {
        float maxY = 0.f;
        float segmentProgress = 0.f;
    }debug;
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
                /*cro::FloatRect textureBounds = m_playerSprite.getComponent<cro::Sprite>().getTextureRect();
                if (ImGui::SliderFloat("Width", &textureBounds.width, 10.f, RenderSizeFloat.x))
                {
                    textureBounds.left = (RenderSizeFloat.x - textureBounds.width) / 2.f;
                    m_playerSprite.getComponent<cro::Sprite>().setTextureRect(textureBounds);
                    m_playerSprite.getComponent<cro::Transform>().setOrigin(glm::vec2(textureBounds.width / 2.f, 0.f));
                }
                if (ImGui::SliderFloat("Height", &textureBounds.height, 10.f, RenderSizeFloat.y))
                {
                    m_playerSprite.getComponent<cro::Sprite>().setTextureRect(textureBounds);
                }
                
                auto pos = m_playerScene.getActiveCamera().getComponent<cro::Transform>().getPosition();
                if (ImGui::SliderFloat("Cam height", &pos.y, 1.f, 6.f))
                {
                    m_playerScene.getActiveCamera().getComponent<cro::Transform>().setPosition(pos);
                }*/

                ImGui::Text("Max Y  %3.3f", debug.maxY);
                ImGui::Text("Player Y  %3.3f", m_player.position.y);
                ImGui::Text("Player Z  %3.3f", m_player.position.z);
                ImGui::Text("Player Segment Progress  %3.3f", debug.segmentProgress);

                static const auto Red = ImVec4(1.f, 0.f, 0.f, 1.f);
                static const auto Green = ImVec4(0.f, 1.f, 0.f, 1.f);
                if (m_inputFlags.flags & InputFlags::Left)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Green);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Red);
                }
                ImGui::Text("Left");
                ImGui::PopStyleColor();

                if (m_inputFlags.flags & InputFlags::Right)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Green);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Red);
                }
                ImGui::Text("Right");
                ImGui::PopStyleColor();

                ImGui::Text("Speed %3.3f", m_player.speed);
                ImGui::Text("PlayerX %3.3f", m_player.position.x);
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
            m_inputFlags.flags |= InputFlags::Up;
            break;
        case SDLK_s:
            m_inputFlags.flags |= InputFlags::Down;
            break;
        case SDLK_a:
            m_inputFlags.flags |= InputFlags::Left;
            break;
        case SDLK_d:
            m_inputFlags.flags |= InputFlags::Right;
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
            m_inputFlags.flags &= ~InputFlags::Up;
            break;
        case SDLK_s:
            m_inputFlags.flags &= ~InputFlags::Down;
            break;
        case SDLK_a:
            m_inputFlags.flags &= ~InputFlags::Left;
            break;
        case SDLK_d:
            m_inputFlags.flags &= ~InputFlags::Right;
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
    updateRoad(dt);
    updatePlayer(dt);

    m_playerScene.simulate(dt);
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void EndlessDrivingState::render()
{
    m_playerTexture.clear(cro::Colour::Transparent/*Blue*/);
    m_playerScene.render();
    m_playerTexture.display();

    m_gameTexture.clear(cro::Colour(std::uint8_t(169), 186, 192));
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
    m_playerTexture.create(RenderSize.x, RenderSize.y);

    auto entity = m_playerScene.createEntity();
    entity.addComponent<cro::Transform>();

    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/cart_v2.cmt"))
    {
        md.createModel(entity);
    }
    m_playerEntity = entity;

    auto resize = [&](cro::Camera& cam)
        {
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setPerspective(cro::Util::Const::degToRad * m_trackCamera.getFOV(), RenderSizeFloat.x/RenderSizeFloat.y, 0.1f, 50.f);
        };

    auto& cam = m_playerScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    //this has to look straight ahead, as that's what we're supposing in the 2D projection
    m_playerScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 3.1f, 5.146f });
    m_playerScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
}

void EndlessDrivingState::createScene()
{
    //background
    auto* tex = &m_resources.textures.get("assets/golf/images/skybox/layers/01.png");
    tex->setRepeated(true);
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -9.5f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(*tex);
    m_background[BackgroundLayer::Sky].entity = entity;
    m_background[BackgroundLayer::Sky].textureRect = entity.getComponent<cro::Sprite>().getTextureRect();
    m_background[BackgroundLayer::Sky].speed = 0.04f;

    tex = &m_resources.textures.get("assets/golf/images/skybox/layers/02.png");
    tex->setRepeated(true);
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -9.4f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(*tex);
    m_background[BackgroundLayer::Hills].entity = entity;
    m_background[BackgroundLayer::Hills].textureRect = entity.getComponent<cro::Sprite>().getTextureRect();
    m_background[BackgroundLayer::Hills].speed = 0.08f;
    m_background[BackgroundLayer::Hills].verticalSpeed = 0.04f;

    tex = &m_resources.textures.get("assets/golf/images/skybox/layers/03.png");
    tex->setRepeated(true);
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -9.3f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(*tex);
    m_background[BackgroundLayer::Trees].entity = entity;
    m_background[BackgroundLayer::Trees].textureRect = entity.getComponent<cro::Sprite>().getTextureRect();
    m_background[BackgroundLayer::Trees].speed = 0.16f;
    m_background[BackgroundLayer::Trees].verticalSpeed = 0.06f;

    //player
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(160.f, 0.f, -0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_playerTexture.getTexture());
    entity.getComponent<cro::Sprite>().setTextureRect(PlayerBounds);
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(PlayerBounds.width / 2.f, 0.f));
    m_playerSprite = entity;

    //road
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -8.f));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.getComponent<cro::Drawable2D>().setCullingEnabled(false); //assume we're always visible and skip bounds checking
    m_roadEntity = entity;


    auto segmentCount = cro::Util::Random::value(5, 20);
    for (auto i = 0; i < segmentCount; ++i)
    {
        const auto enter = cro::Util::Random::value(EnterMin, EnterMax);
        const auto hold = cro::Util::Random::value(HoldMin, HoldMax);
        const auto exit = cro::Util::Random::value(ExitMin, ExitMax);

        const float curve = cro::Util::Random::value(0, 1) ? cro::Util::Random::value(CurveMin, CurveMax) : 0.f;
        const float hill = cro::Util::Random::value(0, 1) ? cro::Util::Random::value(HillMin, HillMax) * SegmentLength : 0.f;
        
        m_road.addSegment(enter, hold, exit, curve, hill);
    }
    m_road[m_road.getSegmentCount() - 1].roadColour = cro::Colour::White;
    m_road[m_road.getSegmentCount() - 1].rumbleColour = cro::Colour::Blue;

    //m_road.addSegment(EnterMin, HoldMin, ExitMin, 0.f, 0.f);

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

void EndlessDrivingState::updatePlayer(float dt)
{
    const float speedRatio = (m_player.speed / Player::MaxSpeed);
    const float dx = dt * 2.f * speedRatio;

    if ((m_inputFlags.flags & (InputFlags::Left | InputFlags::Right)) == 0)
    {
        m_player.model.rotationY *= 1.f - (0.1f * speedRatio);   
    }
    else
    {
        if (m_inputFlags.flags & InputFlags::Left)
        {
            m_player.position.x -= dx;
            m_player.model.rotationY = std::min(Player::Model::MaxY, m_player.model.rotationY + dx);
        }

        if (m_inputFlags.flags & InputFlags::Right)
        {
            m_player.position.x += dx;
            m_player.model.rotationY = std::max(-Player::Model::MaxY, m_player.model.rotationY - dx);
        }
    }
    
    //centrifuge on curves
    static constexpr float Centrifuge = 0.3f;
    const std::size_t segID = static_cast<std::size_t>((m_trackCamera.getPosition().z + m_player.position.z) / SegmentLength);
    m_player.position.x -= dx * speedRatio * m_road[segID].curve * Centrifuge;

   

    if ((m_inputFlags.flags & (InputFlags::Up | InputFlags::Down)) == 0)
    {
        //free wheel decel
        m_player.speed += Player::Deceleration * dt;
    }
    else
    {
        if (m_inputFlags.flags & InputFlags::Up)
        {
            m_player.speed += Player::Acceleration * dt;
        }
        if (m_inputFlags.flags & InputFlags::Down)
        {
            m_player.speed += Player::Braking * dt;
        }
    }

    //is off road
    if ((m_player.position.x < -1.f || m_player.position.x > 1.f)
        && (m_player.speed > Player::OffroadMaxSpeed))
    {
        //slow down
        m_player.speed += Player::OffroadDeceleration * dt;
    }

    m_player.position.x = std::clamp(m_player.position.x, -2.f, 2.f);
    m_player.speed = std::clamp(m_player.speed, 0.f, Player::MaxSpeed);

    m_player.position.z = m_trackCamera.getDepth() * m_trackCamera.getPosition().y;

    const float segmentProgress = ((m_trackCamera.getPosition().z + m_player.position.z) - m_road[segID].position.z) / SegmentLength;
    debug.segmentProgress = segmentProgress;

    const auto nextID = (segID - 1) % m_road.getSegmentCount();
    m_player.position.y = glm::mix(m_road[segID].position.y, m_road[nextID].position.y, segmentProgress);


    glm::vec2 p2(SegmentLength, m_road[nextID].position.y - m_road[segID].position.y);
    m_player.model.targetRotationX = std::atan2(-p2.y, p2.x);

    m_player.model.rotationX += cro::Util::Maths::shortestRotation(m_player.model.rotationX, m_player.model.targetRotationX) * dt;

    //rotate player model with steering
    glm::quat r = glm::toQuat(glm::orientate3(glm::vec3(0.f, 0.f, m_player.model.rotationY)));
    glm::quat s = glm::toQuat(glm::orientate3(glm::vec3(m_player.model.rotationX, 0.f, 0.f)));
    m_playerEntity.getComponent<cro::Transform>().setRotation(s * r);
}

void EndlessDrivingState::updateRoad(float dt)
{
    const float s = m_player.speed /*Player::MaxSpeed*/;
    
    m_trackCamera.move(glm::vec3(0.f, 0.f, s * dt));

    const float maxLen = m_road.getSegmentCount() * SegmentLength;
    if (m_trackCamera.getPosition().z > maxLen)
    {
        m_trackCamera.move(glm::vec3(0.f, 0.f, - maxLen));
    }

    const std::size_t start = static_cast<std::size_t>((m_trackCamera.getPosition().z + m_player.position.z) / SegmentLength);
    float x = 0.f;
    float dx = 0.f;
    auto camPos = m_trackCamera.getPosition();
    float maxY = 0.f;

    std::vector<cro::Vertex2D> verts;
    const float halfWidth = RenderSizeFloat.x / 2.f;

    const auto trackHeight = m_road[start % m_road.getSegmentCount()].position.y;
    m_trackCamera.move(glm::vec3(0.f, trackHeight, 0.f));

    auto prev = m_road[(start - 1) % m_road.getSegmentCount()];
    for (auto i = start; i < start + DrawDistance; ++i)
    {
        const auto& curr = m_road[i % m_road.getSegmentCount()];

        if (i - 1 >= m_road.getSegmentCount())
        {
            m_trackCamera.setZ(camPos.z - (m_road.getSegmentCount() * SegmentLength));
        }
        m_trackCamera.setX(camPos.x - x);
        auto playerPos = m_player.position;
        playerPos.x *= prev.width;
        auto prevProj = m_trackCamera.getScreenProjection(prev, playerPos, RenderSizeFloat);


        //increment
        dx += curr.curve;
        x += dx;

        if (i >= m_road.getSegmentCount())
        {
            m_trackCamera.setZ(camPos.z - (m_road.getSegmentCount() * SegmentLength));
        }
        m_trackCamera.setX(camPos.x - x);
        playerPos = m_player.position;
        playerPos.x *= curr.width;
        auto currProj = m_trackCamera.getScreenProjection(curr, playerPos, RenderSizeFloat);

        prev = curr;

        //cull OOB segments
        if ((prev.position.z < m_trackCamera.getPosition().z)
            || currProj.y < maxY)
        {
            continue;
        }
        maxY = currProj.y;
        debug.maxY = maxY;

        //update vertex array
        float fogAmount = expFog(static_cast<float>(i - start) / DrawDistance, fogDensity);
        
        //grass
        auto colour = glm::mix(curr.grassColour.getVec4(), FogColour.getVec4(), fogAmount);
        addRoadQuad(halfWidth, halfWidth, prevProj.y, currProj.y, halfWidth, halfWidth, colour, verts);

        //rumble strip
        colour = glm::mix(curr.rumbleColour.getVec4(), FogColour.getVec4(), fogAmount);
        addRoadQuad(prevProj.x, currProj.x, prevProj.y, currProj.y, prevProj.z * 1.1f, currProj.z * 1.1f, curr.rumbleColour, verts);

        //road
        colour = glm::mix(curr.roadColour.getVec4(), FogColour.getVec4(), fogAmount);
        addRoadQuad(prevProj.x, currProj.x, prevProj.y, currProj.y, prevProj.z, currProj.z, curr.roadColour, verts);

        //markings
        if (curr.roadMarking)
        {
            addRoadQuad(prevProj.x, currProj.x, prevProj.y, currProj.y, prevProj.z * 0.02f, currProj.z * 0.02f, CD32::Colours[CD32::BeigeLight], verts);
        }

    }
    //addRoadQuad(320.f, 320.f, 0.f, 100.f, 320.f, 320.f, cro::Colour::Magenta, verts);
    m_roadEntity.getComponent<cro::Drawable2D>().getVertexData().swap(verts);


    //update the background
    const float speedRatio = s / Player::MaxSpeed;
    for (auto& layer : m_background)
    {
        layer.textureRect.left += layer.speed * m_road[start].curve * speedRatio;
        layer.textureRect.bottom = layer.verticalSpeed * m_player.position.y * 0.05f;
        layer.entity.getComponent<cro::Sprite>().setTextureRect(layer.textureRect);
    }

    m_trackCamera.setPosition(camPos);
}

void EndlessDrivingState::addRoadQuad(float x1, float x2, float y1, float y2, float w1, float w2, cro::Colour c, std::vector<cro::Vertex2D>& dst)
{
    dst.emplace_back(glm::vec2(x1 - w1, y1), c);
    dst.emplace_back(glm::vec2(x2 - w2, y2), c);
    dst.emplace_back(glm::vec2(x1 + w1, y1), c);

    dst.emplace_back(glm::vec2(x1 + w1, y1), c);
    dst.emplace_back(glm::vec2(x2 - w2, y2), c);
    dst.emplace_back(glm::vec2(x2 + w2, y2), c);
}