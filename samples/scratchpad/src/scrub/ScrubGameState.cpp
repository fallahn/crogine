//Auto-generated source file for Scratchpad Stub 10/10/2024, 12:10:42

#include "ScrubGameState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/Font.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    constexpr float BallRadius = 0.021f;
    constexpr float StrokeDistance = 0.16f - BallRadius;
    constexpr float BallOffsetPos = 0.2f;

    constexpr std::int32_t MaxSoapBars = 5; //don't add if counter is this much

#ifdef USE_GNS
    static_assert(false, "Include the colour file");
#else
    namespace CD32
    {
        static inline constexpr std::array<cro::Colour, 32u> Colours =
        {
            cro::Colour(0xd0b0dff),
            cro::Colour(0xfff8e1ff),
            cro::Colour(0xc8b89fff),
            cro::Colour(0x987a68ff),
            cro::Colour(0x674949ff),
            cro::Colour(0x3a3941ff),
            cro::Colour(0x6b6f72ff),
            cro::Colour(0xadb9b8ff),
            cro::Colour(0xadd9b7ff),
            cro::Colour(0x6eb39dff),
            cro::Colour(0x30555bff),
            cro::Colour(0x1a1e2dff),
            cro::Colour(0x284e43ff),
            cro::Colour(0x467e3eff),
            cro::Colour(0x93ab52ff),
            cro::Colour(0xf2cf5cff),
            cro::Colour(0xec773dff),
            cro::Colour(0xb83530ff),
            cro::Colour(0x722030ff),
            cro::Colour(0x281721ff),
            cro::Colour(0x6d2944ff),
            cro::Colour(0xc85257ff),
            cro::Colour(0xec9983ff),
            cro::Colour(0xdbaf77ff),
            cro::Colour(0xb77854ff),
            cro::Colour(0x833e35ff),
            cro::Colour(0x50282fff),
            cro::Colour(0x65432fff),
            cro::Colour(0x7e6d37ff),
            cro::Colour(0x6ebe70ff),
            cro::Colour(0xb75834ff),
            cro::Colour(0xd55c4dff),
        };

        enum
        {
            Black, BeigeLight, BeigeMid, BeigeDark, BeigeDarkest,
            GreyDark, GreyMid, GreyLight, BlueLight, BlueMid, BlueDark,
            BlueDarkest, GreenDark, GreenMid, GreenLight, Yellow, Orange,
            Red, RedDark, MauveDark, Mauve, Pink, PinkLight, TanLight,
            TanMid, TanDark, TanDarkest, Brown, Olive, Cyan, OrangeDirt,
            PinkDirt,

            Count
        };
    }
#endif

    std::vector<float> scrubTimes;

    /*
    Handle default position is 0 on y
    and -StrokeDistance when fully inserted
    */

    //hacky placeholder for future input
    struct InputBinding
    {
        enum
        {
            PrevClub, NextClub, Left, Right,
            Action,
            Count
        };
        std::array<std::int32_t, Count> keys = { SDLK_q, SDLK_e, SDLK_a, SDLK_d, SDLK_SPACE };
    };
    struct SharedData
    {
        InputBinding inputBinding;
    }m_sharedData;

    struct FontID final
    {
        enum
        {
            UI, Info,

            Count
        };
    };
}

ScrubGameState::ScrubGameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_soapCount     (3)
{
    //this is a pre-cached state
    //context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    //});
}

ScrubGameState::~ScrubGameState()
{
    LogI << "Remove me" << std::endl;
    float total = 0.f;
    for (auto t : scrubTimes)
    {
        total += t;
    }
    total /= scrubTimes.size();
    LogI << "Avg time: " << total << std::endl;
}

//public
bool ScrubGameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return false;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            //requestStackPush(StateID::ScrubPauseState);
            break;
        }

        if (m_score.gameRunning)
        {
            //TODO these should all be moved to funcs so we can also use controller input
            if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Left])
            {
                m_ball.filth = std::max(0.f, m_ball.filth - m_handle.switchDirection(Handle::Down));
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Right])
            {
                m_ball.filth = std::max(0.f, m_ball.filth - m_handle.switchDirection(Handle::Up));
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                //insert ball
                if (m_handle.progress == 0
                    && m_handle.speed == 0
                    && !m_handle.hasBall)
                {
                    m_handle.locked = true;

                    m_ball.state = Ball::State::Insert;
                }
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
            {
                if (m_handle.progress == 0
                    && m_handle.speed == 0
                    && m_handle.hasBall)
                {
                    m_handle.hasBall = false;
                    m_ball.state = Ball::State::Extract;

                    //this *should* work because the models *should* all be at 0,0,0
                    m_handle.entity.getComponent<cro::Transform>().removeChild(m_ball.entity.getComponent<cro::Transform>());
                }
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
            {
                if (m_soapCount)
                {
                    m_soapCount--;
                    m_handle.soap.refresh();
                }
            }
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void ScrubGameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool ScrubGameState::simulate(float dt)
{
    if (m_score.gameRunning)
    {
        m_score.remainingTime = std::max(m_score.remainingTime - dt, 0.f);
        if (m_score.remainingTime == 0)
        {
            //game over, show scores.
            const auto& font = m_resources.fonts.get(FontID::UI);

            glm::vec2 size(cro::App::getWindow().getSize());
            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y / 2.f });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(font).setString("Game Over");
            entity.getComponent<cro::Text>().setCharacterSize(8 * 4);
            entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

            m_score.gameRunning = false;
        }
    }

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void ScrubGameState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void ScrubGameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void ScrubGameState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/hills.hdr");

    //TODO remove this for shared font
    m_resources.fonts.load(FontID::UI, "assets/golf/fonts/IBM_CGA.ttf");
}

void ScrubGameState::createScene()
{
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    if (md.loadFromFile("assets/arcade/scrub/models/handle.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = 
            std::bind(&ScrubGameState::handleCallback, this, std::placeholders::_1, std::placeholders::_2);

        m_handle.entity = entity;
    }

    if (md.loadFromFile("assets/arcade/scrub/models/ball.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -BallOffsetPos, 0.f, 0.f });
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            std::bind(&ScrubGameState::ballCallback, this, std::placeholders::_1, std::placeholders::_2);

        m_ball.entity = entity;
    }

    if (md.loadFromFile("assets/arcade/scrub/models/body.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
    }

    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto camera = m_gameScene.getActiveCamera();
    auto& cam = camera.getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    camera.getComponent<cro::Transform>().setPosition({ 0.f, 0.05f, 0.25f });
    camera.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.1f);

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -1.2f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.6f);
}

void ScrubGameState::createUI()
{
    registerWindow([&]() 
        {
            ImGui::Begin("Buns");

            ImGui::Text("Remaining Time: %3.2f", m_score.remainingTime);
            ImGui::Text("Balls Washed: %d", m_score.ballsWashed);
            ImGui::Text("Avg Cleanliness %3.2f", m_score.avgCleanliness);

            ImGui::NewLine();
            ImGui::Separator();
            ImGui::NewLine();

            ImGui::Text("Handle Speed: %3.2f", m_handle.speed);
            ImGui::Text("Handle Direction: %3.2f", m_handle.direction);

            //TODO make this steeper as we progress in the game (idk, water is getting dirtier or something)
            ImGui::Text("Handle Stroke: %3.2f", cro::Util::Easing::easeInQuad(m_handle.stroke));
            ImGui::Text("Handle Progress: %3.2f", m_handle.progress);

            ImGui::NewLine();
            ImGui::Separator();
            ImGui::NewLine();

            ImGui::Text("Ball Filth: %3.2f", m_ball.filth);
            ImGui::Text("Soap Level: %3.2f", m_handle.soap.amount);
            ImGui::Text("Soap Bars: %d", m_soapCount);
            ImGui::Text("Soap LifeTime %3.3f", m_handle.soap.lifeTime);
            ImGui::Text("Soap Reduction %3.3f", m_handle.soap.getReduction());


            static float oldFilth = 100.f;
            if (m_ball.filth == 0 && oldFilth != 0)
            {
                scrubTimes.push_back(soapTimer.elapsed().asSeconds());
            }
            oldFilth = m_ball.filth;

            ImGui::End();
        });


    //placeholder count-in
    const auto& font = m_resources.fonts.get(FontID::UI);

    glm::vec2 size(cro::App::getWindow().getSize());
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("READY");
    entity.getComponent<cro::Text>().setCharacterSize(8 * 4);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

    struct MessageData final
    {
        float t = 2.f;
        std::int32_t state = 0;
    };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<MessageData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [t, state] = e.getComponent<cro::Callback>().getUserData<MessageData>();
            t -= dt;
            if (state == 0)
            {
                //ready
                if (t < 0)
                {
                    t += 2.f;
                    state = 1;
                    e.getComponent<cro::Text>().setString("GO!");

                    m_score.gameRunning = true;
                }
            }
            else
            {
                //go
                if (t < 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_uiScene.destroyEntity(e);
                }
            }
        };


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);

        //TODO send messge to UI elements to reposition them
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

void ScrubGameState::handleCallback(cro::Entity e, float dt)
{
    m_handle.progress = std::clamp(m_handle.progress + (m_handle.speed * -m_handle.direction * dt), 0.f, 1.f);

    auto pos = e.getComponent<cro::Transform>().getPosition();
    pos.y = cro::Util::Easing::easeOutSine(m_handle.progress) * -StrokeDistance;
    e.getComponent<cro::Transform>().setPosition(pos);

    if (m_handle.progress == 0 || m_handle.progress == 1)
    {
        if (m_handle.speed != 0)
        {
            m_ball.filth = std::max(0.f, m_ball.filth - m_handle.calcStroke());
        }
        m_handle.speed = 0.f;
    }
}

void ScrubGameState::ballCallback(cro::Entity e, float dt)
{
    switch (m_ball.state)
    {
    default:
    case Ball::State::Idle:
        break;
    case Ball::State::Insert:
    {
        auto pos = m_ball.entity.getComponent<cro::Transform>().getPosition();
        pos.x = std::min(0.f, pos.x + Ball::Speed * dt);
        m_ball.entity.getComponent<cro::Transform>().setPosition(pos);

        if (pos.x == 0)
        {
            m_handle.entity.getComponent<cro::Transform>().addChild(m_ball.entity.getComponent<cro::Transform>());
            m_handle.locked = false;
            m_handle.hasBall = true;
            m_ball.state = Ball::State::Clean;

            soapTimer.restart();
        }
    }
    break;
    case Ball::State::Clean:
        m_handle.soap.lifeTime += dt;
        break;
    case Ball::State::Extract:
    {
        auto pos = m_ball.entity.getComponent<cro::Transform>().getPosition();
        pos.x += Ball::Speed * dt;

        if (pos.x > BallOffsetPos)
        {
            updateScore();

            m_ball.state = Ball::State::Idle;
            m_ball.filth = 100.f;
            pos.x = -BallOffsetPos;

            m_ball.colourIndex += cro::Util::Random::value(1, 3);
            m_ball.colourIndex %= CD32::Colours.size();
            m_ball.entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", CD32::Colours[m_ball.colourIndex]);
        }
        m_ball.entity.getComponent<cro::Transform>().setPosition(pos);
    }
    break;
    }
}

void ScrubGameState::updateScore()
{
    const float cleanliness = 100.f - m_ball.filth;

    m_score.ballsWashed++;
    m_score.cleanlinessSum += cleanliness;
    m_score.avgCleanliness = m_score.cleanlinessSum / m_score.ballsWashed;

    //TODO penalise time for very grubby balls?
    m_score.remainingTime += Score::TimeBonus * (cleanliness / 100.f);



    //TODO track bonus runs of 3x 100%, 5x 100% and 10x 100%

    //TODO develop some actual scoring system and calc the score

    //TODO every X balls is a new soap, or every X points?


    //TODO increase the scub curve - I've forgotten what I meant by this...
}

//handle funcs
float ScrubGameState::Handle::switchDirection(float d)
{
    CRO_ASSERT(d == Down || d == Up, "");

    float ret = 0.f;
    if (d != direction
        && !locked)
    {
        //do this first as it uses the current direction
        ret = calcStroke();

        direction = d;
        speed = MaxSpeed;

        if (hasBall)
        {
            soap.amount = std::max(1.f, soap.amount - soap.getReduction());
        }
    }
    return ret;
}

float ScrubGameState::Handle::calcStroke()
{
    const float currPos = entity.getComponent<cro::Transform>().getPosition().y;
    stroke = ((currPos - strokeStart) / StrokeDistance) * direction;
    strokeStart = currPos;

    if (hasBall)
    {
        return stroke * soap.amount;
    }
    return 0.f;
}