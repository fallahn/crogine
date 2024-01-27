//Auto-generated source file for Scratchpad Stub 08/01/2024, 12:14:56

#include "EndlessDrivingState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Wavetable.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtx/euler_angles.hpp>

#include <sstream>
#include <iomanip>

namespace
{
    constexpr std::uint32_t RenderScale = 2;
    constexpr glm::uvec2 RenderSize = glm::uvec2(320, 224) * RenderScale;
    constexpr glm::vec2 RenderSizeFloat = glm::vec2(RenderSize);
    constexpr float ScreenHalfWidth = RenderSizeFloat.x / 2.f;
    constexpr float PlayerWidth = 74.f * RenderScale;
    constexpr float PlayerHeight = 84.f * RenderScale;
    constexpr cro::FloatRect PlayerBounds((RenderSizeFloat.x - PlayerWidth) / 2.f, 0.f, PlayerWidth, PlayerHeight);

    constexpr float FogDensity = 15.f;
    float expFog(float distance, float density)
    {
        float fogAmount = 1.f / (std::pow(cro::Util::Const::E, (distance * distance * density)));
        fogAmount = (0.5f * (1.f - fogAmount));

        fogAmount = std::round(fogAmount * 25.f);
        fogAmount /= 25.f;

        return fogAmount;
    }
    const cro::Colour GrassFogColour = CD32::Colours[CD32::GreenDark];
    const cro::Colour RoadFogColour = CD32::Colours[CD32::BlueDark];

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

                ImGui::ProgressBar(m_inputFlags.brakeMultiplier);
                ImGui::ProgressBar(m_inputFlags.accelerateMultiplier);
                ImGui::ProgressBar(m_inputFlags.steerMultiplier);

                ImGui::Text("Key Count %d", m_inputFlags.keyCount);

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
        }

        if (!evt.key.repeat)
        {
            //TODO compare to keybind
            if (evt.key.keysym.sym == SDLK_w)
            {
                m_inputFlags.flags |= InputFlags::Up;
                m_inputFlags.keyCount++;
            }
            if (evt.key.keysym.sym == SDLK_s)
            {
                m_inputFlags.flags |= InputFlags::Down;
                m_inputFlags.keyCount++;
            }
            if (evt.key.keysym.sym == SDLK_a)
            {
                m_inputFlags.flags |= InputFlags::Left;
                m_inputFlags.keyCount++;
            }
            if (evt.key.keysym.sym == SDLK_d)
            {
                m_inputFlags.flags |= InputFlags::Right;
                m_inputFlags.keyCount++;
            }
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        //TODO compare to keybind
        if (evt.key.keysym.sym == SDLK_w)
        {
            m_inputFlags.flags &= ~InputFlags::Up;
            m_inputFlags.keyCount--;
        }
        if (evt.key.keysym.sym == SDLK_s)
        {
            m_inputFlags.flags &= ~InputFlags::Down;
            m_inputFlags.keyCount--;
        }
        if (evt.key.keysym.sym == SDLK_a)
        {
            m_inputFlags.flags &= ~InputFlags::Left;
            m_inputFlags.keyCount--;
        }
        if (evt.key.keysym.sym == SDLK_d)
        {
            m_inputFlags.flags &= ~InputFlags::Right;
            m_inputFlags.keyCount--;
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
    updateControllerInput();
    if (m_gameRules.state == GameRules::State::Running)
    {
        //TODO update vehicles
        if (getStateCount() != 1)
        {
            m_inputFlags.flags = 0;
        }
        else
        {
            m_gameRules.remainingTime -= dt;
            m_gameRules.remainingTime = std::max(0.f, m_gameRules.remainingTime);
            
            //TODO some sort of flashing warning if time is low

            if(m_gameRules.remainingTime == 0)
            {
                //TODO push game over state
            }
            m_gameRules.totalTime += dt;
        }
    }
    else
    {
        m_inputFlags.flags = 0;
    }
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

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void EndlessDrivingState::loadAssets()
{
    m_wavetables[TrackSprite::Animation::Rotate] = cro::Util::Wavetable::sine(1.5f);// used to animate sprites
    for (auto& f : m_wavetables[TrackSprite::Animation::Rotate])
    {
        f += 1.f;
        f /= 2.f;
    }
    m_wavetables[TrackSprite::Animation::Float] = cro::Util::Wavetable::sine(0.5f, 20.f);

    //everything needs to be in a single sprite sheet as we're
    //relying on draw order for depth sorting.
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/collectibles.spt", m_resources.textures);

    const auto parseSprite = 
        [&](const cro::Sprite& sprite, std::int32_t spriteID, std::int32_t animation)
        {
            auto bounds = sprite.getTextureBounds();
            m_trackSprites[spriteID].size = { bounds.width, bounds.height };
            m_trackSprites[spriteID].uv = sprite.getTextureRectNormalised();
            m_trackSprites[spriteID].id = spriteID;
            m_trackSprites[spriteID].animation = animation;
        };

    parseSprite(spriteSheet.getSprite("tree01"), TrackSprite::Tree01, 0);
    parseSprite(spriteSheet.getSprite("bush01"), TrackSprite::Bush01, 0);
    parseSprite(spriteSheet.getSprite("ball"), TrackSprite::Ball, TrackSprite::Animation::Rotate);
    parseSprite(spriteSheet.getSprite("flag"), TrackSprite::Flag, TrackSprite::Animation::Float);


    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -7.f });
    entity.addComponent<cro::Drawable2D>().setTexture(spriteSheet.getTexture());
    entity.getComponent<cro::Drawable2D>().setCullingEnabled(false);
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setVertexData({ cro::Vertex2D() }); //there's a crash on nvidia drivers if we don't initialise this with at least 1 vertex...
    m_trackSpriteEntity = entity;


    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.5f });
    entity.addComponent<cro::Drawable2D>().setCullingEnabled(false);
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setVertexData({ cro::Vertex2D() });
    m_debugEntity = entity;
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
    static constexpr float BGScale = static_cast<float>(RenderScale) / 2.f;

    //background
    auto* tex = &m_resources.textures.get("assets/golf/images/skybox/layers/01.png");
    tex->setRepeated(true);
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -9.5f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(BGScale));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(*tex);
    m_background[BackgroundLayer::Sky].entity = entity;
    m_background[BackgroundLayer::Sky].textureRect = entity.getComponent<cro::Sprite>().getTextureRect();
    m_background[BackgroundLayer::Sky].speed = 40.f;

    tex = &m_resources.textures.get("assets/golf/images/skybox/layers/02.png");
    tex->setRepeated(true);
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -9.4f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(BGScale));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(*tex);
    m_background[BackgroundLayer::Hills].entity = entity;
    m_background[BackgroundLayer::Hills].textureRect = entity.getComponent<cro::Sprite>().getTextureRect();
    m_background[BackgroundLayer::Hills].speed = 80.f;
    m_background[BackgroundLayer::Hills].verticalSpeed = 40.f;

    tex = &m_resources.textures.get("assets/golf/images/skybox/layers/03.png");
    tex->setRepeated(true);
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -9.3f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(BGScale));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(*tex);
    m_background[BackgroundLayer::Trees].entity = entity;
    m_background[BackgroundLayer::Trees].textureRect = entity.getComponent<cro::Sprite>().getTextureRect();
    m_background[BackgroundLayer::Trees].speed = 160.f;
    m_background[BackgroundLayer::Trees].verticalSpeed = 60.f;

    //player
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(RenderSizeFloat.x / 2.f, 0.f, -0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_playerTexture.getTexture());
    entity.getComponent<cro::Sprite>().setTextureRect(PlayerBounds);
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(PlayerBounds.width / 2.f, 0.f));
    m_playerSprite = entity;

    //road
    auto& noiseTex = m_resources.textures.get("assets/golf/images/track_noise.png");
    noiseTex.setRepeated(true);

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -8.f));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.getComponent<cro::Drawable2D>().setCullingEnabled(false); //assume we're always visible and skip bounds checking
    entity.getComponent<cro::Drawable2D>().setTexture(&noiseTex);
    entity.getComponent<cro::Drawable2D>().setVertexData({ cro::Vertex2D() });
    m_roadEntity = entity;

    createRoad();    


    auto resize = [](cro::Camera& cam)
    {
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setOrthographic(0.f, static_cast<float>(RenderSize.x), 0.f, static_cast<float>(RenderSize.y), -0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

void EndlessDrivingState::createRoad()
{
    //TODO create a constant start/end segment DrawDistance in size
    //TODO add UV coords for ground texture

    const std::vector<std::int32_t> SwapPattern =
    {
        cro::Util::Random::value(10, 16),
        cro::Util::Random::value(6, 10),
        cro::Util::Random::value(10, 16),
        cro::Util::Random::value(10, 20),
        cro::Util::Random::value(6, 10),
    };
    std::int32_t swapIndex = 0;
    std::int32_t swapCounter = 0;
    float offsetMultiplier = 1.f;

    //offset the start frame of anumations so not all in sync (looks weird)
    std::array<std::size_t, TrackSprite::Animation::Count> animationFrameOffsets = {};

    auto segmentCount = cro::Util::Random::value(5, 20);
    for (auto i = 0; i < segmentCount; ++i)
    {
        const std::size_t first = m_road.getSegmentCount();

        const auto enter = cro::Util::Random::value(EnterMin, EnterMax);
        const auto hold = cro::Util::Random::value(HoldMin, HoldMax);
        const auto exit = cro::Util::Random::value(ExitMin, ExitMax);

        const std::size_t last = first + enter + hold + exit;

        const float curve = cro::Util::Random::value(0, 1) ? cro::Util::Random::value(CurveMin, CurveMax) : 0.f;
        const float hill = cro::Util::Random::value(0, 1) ? cro::Util::Random::value(HillMin, HillMax) * SegmentLength : 0.f;

        m_road.addSegment(enter, hold, exit, curve, hill);


        //add sprites to the new segment
        for (auto j = first; j < last; ++j)
        {
            auto& seg = m_road[j];

            //road side foliage
            auto count = cro::Util::Random::value(0, 2) == 0 ? 1 : 0;
            for (auto k = 0; k < count; ++k)
            {
                auto spriteID = cro::Util::Random::value(TrackSprite::Tree01, TrackSprite::Bush01);
                auto pos = -1.75f - cro::Util::Random::value(0.15f, 0.3f);
                if (cro::Util::Random::value(0, 1) == 0)
                {
                    pos *= -1.f;
                }
                seg.sprites.emplace_back(m_trackSprites[spriteID]).position = pos;
                seg.sprites.back().scale *= cro::Util::Random::value(0.9f, 1.8f) * RenderScale;
            }

            //collectibles
            if (j - first > enter
                && j < last - exit)
            {
                if (j % 16 == 0)
                {
                    auto& spr = seg.sprites.emplace_back(m_trackSprites[TrackSprite::Ball]);
                    spr.position = 0.5f * offsetMultiplier;
                    spr.scale = 2.5f;
                    spr.frameIndex = animationFrameOffsets[spr.animation];

                    animationFrameOffsets[spr.animation] = (animationFrameOffsets[spr.animation] + 5) % m_wavetables[spr.animation].size();

                    //counts the generated slices and swaps
                    //the position of collectibles based on SwapPattern
                    swapCounter++;
                    if (swapCounter == SwapPattern[swapIndex])
                    {
                        swapCounter = 0;
                        swapIndex = (swapIndex + 1) % SwapPattern.size();
                        offsetMultiplier *= -1.f;

                        spr.position = 0.f; //smoother transition
                    }
                }
            }

            if ((j == last - exit) && (i % 5) == 4)
            {
                auto& spr = seg.sprites.emplace_back(m_trackSprites[TrackSprite::Flag]);
                spr.position = -0.5f * offsetMultiplier;
                spr.scale = 3.5f;
                spr.frameIndex = animationFrameOffsets[spr.animation];

                animationFrameOffsets[spr.animation] = (animationFrameOffsets[spr.animation] + 5) % m_wavetables[spr.animation].size();
            }
        }
    }
    m_road[m_road.getSegmentCount() - 1].roadColour = cro::Colour::White;
    m_road[m_road.getSegmentCount() - 1].rumbleColour = cro::Colour::Blue;
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


    //TODO switch font to shared resources
    std::uint32_t fontID = 2;
    m_resources.fonts.load(fontID, "assets/golf/fonts/IBM_CGA.ttf");
    auto& font = m_resources.fonts.get(fontID);

    //create a count-in
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(RenderSizeFloat.x / 2.f, std::floor(RenderSizeFloat.y * 0.75f), 0.1f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f)); //hide until menu popped
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("3");
    entity.getComponent<cro::Text>().setCharacterSize(8 * 10);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::Red]);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(1.f, 3);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            if (getStateCount() == 1)
            {
                auto& [currTime, count] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
                currTime -= dt;

                if (currTime < 0.f)
                {
                    currTime += 1.f;
                    count--;
                    if (count)
                    {
                        if (count == -1)
                        {
                            e.getComponent<cro::Callback>().active = false;
                            m_uiScene.destroyEntity(e);
                        }
                        else
                        {
                            e.getComponent<cro::Text>().setString(std::to_string(count));
                        }
                    }
                    else
                    {
                        //TODO check if accelerate was being held and set input flag

                        e.getComponent<cro::Text>().setString("GO!");
                        m_gameRules.state = GameRules::State::Running;
                    }
                }
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            }
        };

    m_gameEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //time display
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(12.f, std::floor(RenderSizeFloat.y * 0.95f), 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(16);
    entity.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::Red]);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            std::stringstream stream;
            stream << std::fixed << std::setprecision(2) 
                << "Total: " << m_gameRules.totalTime << "\n"
                << "Remaining: " << m_gameRules.remainingTime;
            
            std::string str = stream.str();
            e.getComponent<cro::Text>().setString(str);
        };
    m_gameEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

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

void EndlessDrivingState::updateControllerInput()
{
    m_inputFlags.steerMultiplier = 1.f;
    m_inputFlags.accelerateMultiplier = 1.f;
    m_inputFlags.brakeMultiplier = 1.f;

    if (m_inputFlags.keyCount)
    {
        //some keys are pressed, don't use controller
        return;
    }


    auto axisPos = cro::GameController::getAxisPosition(0, cro::GameController::TriggerRight);
    if (axisPos > cro::GameController::TriggerDeadZone)
    {
        m_inputFlags.accelerateMultiplier = cro::Util::Easing::easeInCubic(static_cast<float>(axisPos) / cro::GameController::AxisMax);
        m_inputFlags.flags |= InputFlags::Up;
    }
    else
    {
        //hmm, how to check if the keys aren't being used?
        m_inputFlags.flags &= ~InputFlags::Up;
    }

    axisPos = cro::GameController::getAxisPosition(0, cro::GameController::TriggerLeft);
    if (axisPos > cro::GameController::TriggerDeadZone)
    {
        m_inputFlags.brakeMultiplier = static_cast<float>(axisPos) / cro::GameController::AxisMax;
        m_inputFlags.flags |= InputFlags::Down;
    }
    else
    {
        m_inputFlags.flags &= ~InputFlags::Down;
    }

    axisPos = cro::GameController::getAxisPosition(0, cro::GameController::AxisLeftX);
    if (axisPos > cro::GameController::LeftThumbDeadZone)
    {
        m_inputFlags.steerMultiplier = cro::Util::Easing::easeInCubic(static_cast<float>(axisPos) / cro::GameController::AxisMax);
        m_inputFlags.flags |= InputFlags::Right;
        m_inputFlags.flags &= ~InputFlags::Left;
    }
    else if (axisPos < -cro::GameController::LeftThumbDeadZone)
    {
        m_inputFlags.steerMultiplier = cro::Util::Easing::easeInCubic(static_cast<float>(axisPos) / -cro::GameController::AxisMax);
        m_inputFlags.flags |= InputFlags::Left;
        m_inputFlags.flags &= ~InputFlags::Right;
    }
    else
    {
        m_inputFlags.flags &= ~(InputFlags::Left | InputFlags::Right);
    }
}

void EndlessDrivingState::updatePlayer(float dt)
{
    const float speedRatio = (m_player.speed / Player::MaxSpeed);
    const float dx = dt * 2.f * speedRatio;
    const float dxSteer = dt * 2.f * cro::Util::Easing::easeOutQuad(speedRatio);

    if ((m_inputFlags.flags & (InputFlags::Left | InputFlags::Right)) == 0)
    {
        m_player.model.rotationY *= 1.f - (0.1f * speedRatio);   
    }
    else
    {
        if (m_inputFlags.flags & InputFlags::Left)
        {
            const auto d = dxSteer * m_inputFlags.steerMultiplier;
            m_player.position.x -= d;
            m_player.model.rotationY = std::min(Player::Model::MaxY, m_player.model.rotationY + (dx * m_inputFlags.steerMultiplier));
        }

        if (m_inputFlags.flags & InputFlags::Right)
        {
            const auto d = dxSteer * m_inputFlags.steerMultiplier;
            m_player.position.x += d;
            m_player.model.rotationY = std::max(-Player::Model::MaxY, m_player.model.rotationY - (dx * m_inputFlags.steerMultiplier));
        }
    }
    
    //centrifuge on curves
    const std::size_t segID = static_cast<std::size_t>((m_trackCamera.getPosition().z + m_player.position.z) / SegmentLength) % m_road.getSegmentCount();
    m_player.position.x -= dx * cro::Util::Easing::easeInCubic(speedRatio) * m_road[segID].curve * Player::Centrifuge;

   

    if ((m_inputFlags.flags & (InputFlags::Up | InputFlags::Down)) == 0)
    {
        //free wheel decel
        m_player.speed += Player::Deceleration * dt;
    }
    else
    {
        if (m_inputFlags.flags & InputFlags::Up)
        {
            m_player.speed += Player::Acceleration * dt * m_inputFlags.accelerateMultiplier;
        }
        if (m_inputFlags.flags & InputFlags::Down)
        {
            m_player.speed += Player::Braking * dt *  m_inputFlags.brakeMultiplier;
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




    //collect the IDs of the next 3 segs to test collision on 
    //(just one seg can miss collisions because of tunnelling)
    std::vector<std::size_t> collisionSegs;
    auto collisionID = segID;
    for (auto i = 0; i < 3; ++i)
    {
        collisionSegs.push_back(collisionID);
        collisionID = (collisionID + 1) % m_road.getSegmentCount();
    }

    std::vector<cro::Vertex2D> debugVerts;
    auto playerBounds = m_playerSprite.getComponent<cro::Transform>().getWorldTransform() * m_playerSprite.getComponent<cro::Drawable2D>().getLocalBounds();
    auto p = glm::vec2(playerBounds.left, playerBounds.bottom);
    auto w = glm::vec2(playerBounds.width, playerBounds.height) * 0.65f; //better fit sprite bounds for collision
    p.x += (playerBounds.width - w.x) / 2.f;
    
    const cro::FloatRect PlayerCollision(p, w);

    //test each sprite in each collision segment for player collision
    for (auto i : collisionSegs)
    {
        for (auto& spr : m_road[i].sprites)
        {
            if (spr.collisionActive)
            {
                auto [pos, size] = getScreenCoords(spr, m_road[segID], false);

                if (PlayerCollision.intersects({ pos, size }))
                {
                    switch (spr.id)
                    {
                    default:break;
                    case TrackSprite::Flag:
                        //BEEF STICK
                        LogI << "Beef Stick!" << std::endl;
                        break;
                    case TrackSprite::Ball:
                        //seconds
                        LogI << "Kerching" << std::endl;
                        break;
                    case TrackSprite::Bush01:
                        //slow down
                        LogI << "Hit Bush" << std::endl;
                        break;
                    case TrackSprite::Tree01:
                        //crash
                        LogI << "Hit Tree" << std::endl;
                        break;
                    }
                    spr.collisionActive = false; //prevent multiple collisions
                }
            }
        }
    }
    m_debugEntity.getComponent<cro::Drawable2D>().getVertexData().swap(debugVerts);
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

    const std::size_t start = static_cast<std::size_t>((m_trackCamera.getPosition().z + m_player.position.z) / SegmentLength) - 1;
    float x = 0.f;
    float dx = 0.f;
    auto camPos = m_trackCamera.getPosition();
    float maxY = 0.f;

    std::vector<cro::Vertex2D> verts;

    const auto trackHeight = m_road[start % m_road.getSegmentCount()].position.y;
    m_trackCamera.move(glm::vec3(0.f, trackHeight, 0.f));

    //keep this because we need to reverse the draw order
    std::vector<std::size_t> spriteSegments;

    auto* prev = &m_road[(start - 1) % m_road.getSegmentCount()];
    
    if (start - 1 >= m_road.getSegmentCount())
    {
        m_trackCamera.setZ(camPos.z - (m_road.getSegmentCount() * SegmentLength));
    }
    auto playerPos = m_player.position;
    playerPos.x *= prev->width;
    m_trackCamera.updateScreenProjection(*prev, playerPos, RenderSizeFloat);  

    for (auto i = start; i < start + DrawDistance; ++i)
    {
        const auto currIndex = i % m_road.getSegmentCount();
        auto& curr = m_road[currIndex];

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
        m_trackCamera.updateScreenProjection(curr, playerPos, RenderSizeFloat);

        float fogAmount = expFog(static_cast<float>(i - start) / DrawDistance, FogDensity);
        curr.fogAmount = fogAmount;

        //stash the sprites - these might poke out from
        //behind a hill so we'll draw them anyway regardless
        //of segment culling
        if (!curr.sprites.empty())
        {
            spriteSegments.push_back(currIndex);
        }

        //cull OOB segments
        if ((prev->position.z < m_trackCamera.getPosition().z)
            || curr.projection.position.y < maxY)
        {
            curr.clipHeight = (curr.projection.position.y < maxY) ? maxY : 0.f;
            continue;
        }
        maxY = curr.projection.position.y;
        curr.clipHeight = 0.f;
        debug.maxY = maxY;

        //update vertex array
        
        //grass
        auto colour = glm::mix(curr.grassColour.getVec4(), GrassFogColour.getVec4(), fogAmount);
        const float uvP = ScreenHalfWidth / (prev->projection.width);
        const float offsetP = ((prev->projection.position.x - ScreenHalfWidth) / ScreenHalfWidth) * -uvP;
        const float uvC = ScreenHalfWidth / (curr.projection.width);
        const float offsetC = ((curr.projection.position.x - ScreenHalfWidth) / ScreenHalfWidth) * -uvC;

        cro::Colour c = colour;
        verts.emplace_back(glm::vec2(0.f, prev->projection.position.y), glm::vec2(-uvP + offsetP, prev->uv.y), c);
        verts.emplace_back(glm::vec2(0.f, curr.projection.position.y), glm::vec2(-uvC + offsetC, curr.uv.y), c);
        verts.emplace_back(glm::vec2(RenderSizeFloat.x, prev->projection.position.y), glm::vec2(uvP + offsetP, prev->uv.y), c);

        verts.emplace_back(glm::vec2(RenderSizeFloat.x, prev->projection.position.y), glm::vec2(uvP + offsetP, prev->uv.y), c);
        verts.emplace_back(glm::vec2(0.f, curr.projection.position.y), glm::vec2(-uvC + offsetC, curr.uv.y), c);
        verts.emplace_back(glm::vec2(RenderSizeFloat.x, curr.projection.position.y), glm::vec2(uvC + offsetC, curr.uv.y), c);



        //rumble strip
        colour = glm::mix(curr.rumbleColour.getVec4(), RoadFogColour.getVec4(), fogAmount);
        addRoadQuad(*prev, curr, 1.1f, colour, verts);

        //road
        colour = glm::mix(curr.roadColour.getVec4(), RoadFogColour.getVec4(), fogAmount);
        addRoadQuad(*prev, curr, 1.f, colour, verts);

        //markings
        if (curr.roadMarking)
        {
            addRoadQuad(*prev, curr, 0.02f, CD32::Colours[CD32::BeigeLight], verts);
        }


        //do this LAST!! buh
        prev = &curr;
    }
    m_roadEntity.getComponent<cro::Drawable2D>().getVertexData().swap(verts);

    verts.clear();
    //hmmm what's faster, reversing the vector, or iterating backwards?
    for (auto i = spriteSegments.crbegin(); i != spriteSegments.crend(); ++i)
    {
        const auto idx = *i;
        auto& seg = m_road[idx];

        for (auto& sprite : seg.sprites)
        {
            addRoadSprite(sprite, seg, verts);
        }
    }
    m_trackSpriteEntity.getComponent<cro::Drawable2D>().getVertexData().swap(verts);

    //reset the collision on sprites which came back into view
    if (!spriteSegments.empty())
    {
        for (auto& spr : m_road[spriteSegments.back()].sprites)
        {
            spr.collisionActive = true;
        }
    }

    //update the background
    const float speedRatio = s / Player::MaxSpeed;
    for (auto& layer : m_background)
    {
        layer.textureRect.left += layer.speed * m_road[start % m_road.getSegmentCount()].curve * speedRatio;
        layer.textureRect.bottom = layer.verticalSpeed * m_player.position.y * 0.05f;
        layer.entity.getComponent<cro::Sprite>().setTextureRect(layer.textureRect);
    }

    m_trackCamera.setPosition(camPos);
}

void EndlessDrivingState::addRoadQuad(const TrackSegment& s1, const TrackSegment& s2, float widthMultiplier, cro::Colour c, std::vector<cro::Vertex2D>& dst)
{
    const auto p1 = s1.projection.position;
    const auto p2 = s2.projection.position;

    const auto w1 = s1.projection.width * widthMultiplier;
    const auto w2 = s2.projection.width * widthMultiplier;

    dst.emplace_back(glm::vec2(p1.x - w1, p1.y), s1.uv, c);
    dst.emplace_back(glm::vec2(p2.x - w2, p2.y), s2.uv, c);
    dst.emplace_back(glm::vec2(p1.x + w1, p1.y), s1.uv + glm::vec2(widthMultiplier, 0.f), c);

    dst.emplace_back(glm::vec2(p1.x + w1, p1.y), s1.uv + glm::vec2(widthMultiplier, 0.f), c);
    dst.emplace_back(glm::vec2(p2.x - w2, p2.y), s2.uv, c);
    dst.emplace_back(glm::vec2(p2.x + w2, p2.y), s2.uv + glm::vec2(widthMultiplier, 0.f), c);
}

void EndlessDrivingState::addRoadSprite(TrackSprite& sprite, const TrackSegment& seg, std::vector<cro::Vertex2D>& dst)
{
    if (!sprite.collisionActive)
    {
        return;
    }

    auto uv = sprite.uv;
    
    auto [pos, size] = getScreenCoords(sprite, seg, sprite.animation != 0);

    cro::Colour c = glm::mix(glm::vec4(1.f), GrassFogColour.getVec4(), seg.fogAmount);
    if (seg.clipHeight > pos.y + size.y)
    {
        //fully occluded
        return;
    }

    if (seg.clipHeight)
    {
        //c = cro::Colour::Magenta;

        const auto diff = seg.clipHeight - pos.y;
        const auto uvOffset = diff / size.y;
        const auto uvDiff = uv.height * uvOffset;

        pos.y += diff;
        size.y -= diff;

        uv.bottom += uvDiff;
        uv.height -= uvDiff;
    }

    dst.emplace_back(glm::vec2(pos.x, pos.y + size.y), glm::vec2(uv.left, uv.bottom + uv.height), c);
    dst.emplace_back(pos, glm::vec2(uv.left, uv.bottom), c);
    dst.emplace_back(pos + size, glm::vec2(uv.left + uv.width, uv.bottom + uv.height), c);

    dst.emplace_back(pos + size, glm::vec2(uv.left + uv.width, uv.bottom + uv.height), c);
    dst.emplace_back(pos, glm::vec2(uv.left, uv.bottom), c);
    dst.emplace_back(glm::vec2(pos.x + size.x, pos.y), glm::vec2(uv.left + uv.width, uv.bottom), c);
}

std::pair<glm::vec2, glm::vec2> EndlessDrivingState::getScreenCoords(TrackSprite& sprite, const TrackSegment& seg, bool animate)
{
    glm::vec2 pos = seg.projection.position;
    pos.x += seg.projection.scale * sprite.position * RoadWidth * ScreenHalfWidth;

    const float scale = seg.projection.scale * sprite.scale;

    glm::vec2 size = sprite.size * scale;

    if (sprite.animation == TrackSprite::Animation::Rotate)
    {
        size.x *= m_wavetables[sprite.animation][sprite.frameIndex];
    }
    else if (sprite.animation == TrackSprite::Animation::Float)
    {
        pos.y += m_wavetables[sprite.animation][sprite.frameIndex] * scale;
    }
    pos.x -= (size.x / 2.f);

    //we mightjust be querying the sprite screen size for collision
    //which won't want to update the animation
    if (animate)
    {
        sprite.frameIndex = (sprite.frameIndex + 1) % m_wavetables[sprite.animation].size();
    }


    return std::make_pair(pos, size);
}