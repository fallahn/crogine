/*-----------------------------------------------------------------------

Matt Marchant - 2025
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "ClubInfoState.hpp"
#include "SharedStateData.hpp"
#include "MenuConsts.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/UIElement.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/graphics/Font.hpp>

ClubInfoState::ClubInfoState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_sharedData    (sd),
    m_messageIndex  (0)
{
    addSystems();
    buildScene();
}

//public
bool ClubInfoState::handleEvent(const cro::Event& evt)
{
    const auto nextMessage =
        [&]()
        {
            if (m_continueNode.getComponent<cro::Transform>().getScale().x != 0)
            {
                m_messageIndex++;

                if (m_messageIndex == m_messages.size())
                {
                    requestStackPop();
                }
                else
                {
                    m_messageNode.getComponent<cro::Text>().setString(m_messages[m_messageIndex]);
                    centreText();

                    m_continueNode.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    m_continueNode.getComponent<cro::Callback>().active = true;
                }
            }
        };

    switch (evt.type)
    {
    default: break;
    case SDL_KEYUP:
        if (evt.key.keysym.sym == SDLK_ESCAPE)
        {
            requestStackPop();
        }
        else
        {
            nextMessage();
        }
        break;
    case SDL_CONTROLLERBUTTONUP:
        if (evt.cbutton.button == cro::GameController::ButtonB)
        {
            requestStackPop();
        }
        else
        {
            nextMessage();
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            nextMessage();
        }
    }

    m_scene.forwardEvent(evt);
    return false;
}

void ClubInfoState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool ClubInfoState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void ClubInfoState::render()
{
    m_scene.render();
}

//private
void ClubInfoState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();

    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::UIElementSystem>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
}

void ClubInfoState::buildScene()
{
    const cro::Colour c(0.f, 0.f, 0.f, /*BackgroundAlpha*/0.9f);

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -2.f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-0.5f, 0.5f), c),
            cro::Vertex2D(glm::vec2(-0.5f), c),
            cro::Vertex2D(glm::vec2(0.5f), c),
            cro::Vertex2D(glm::vec2(0.5f, -0.5f), c)
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            const auto size = glm::vec2(cro::App::getWindow().getSize());
            e.getComponent<cro::Transform>().setPosition(size / 2.f);
            
            auto scale = glm::vec2(e.getComponent<cro::Transform>().getScale());
            scale += (size - scale) * (dt * 12.f);
            e.getComponent<cro::Transform>().setScale(scale);
        };

    m_messages =
    {
R"(Welcome Back Golfer! A little note on the new club sets...

Firstly - all club sets are unlocked from the beginning.
Secondly - all club sets have a full range of 260m/284yds

No more grinding for upgrades - just dive right in!
)",

R"(
So what's changed? Mostly how the abilities are distributed.

The Novice clubs are now Casual clubs. These are the clubs
you knew and loved previously as the Pro clubs. Full range and
easy to use - perfect if all you want to do is bat the ball
up and down the fairways.
)",

R"(
If you fancy more of a challenge, however, then the Expert
clubs are for you! These narrow down the tolerance for an
accurate shot, as well as increasing the CPU opponent difficulty.
)",

R"(
And, if this wasn't challenge enough, the Pro set is now
extra tough! The accuracy margin is incredibly slim requiring
a near perfect stroke every time - and an inaccurate stroke
will cause your shot to fall short, especially from the rough
or a bunker.
)",

R"(
Expert and Pro club sets offer increased rewards in the form
of up to double XP and double clubshop credits for more
difficult shots. And, for those so inclined, the Expert and
Pro sets can be customised and fine tuned from the Equipment
Counter - available in the Clubhouse or Roster menu.
)",

R"(
As Casual clubs are essentially fully buffed, custom equipment
will make no difference to them. However a new set of balls or
a high reflex club can make all the difference to your Pro game!
Assign any new equipment you acquire using the Loadout button
in the Profile Editor.
)",

R"(
That's it! Good luck out there and, as always,

Happy Golfing! )"
    };

#ifdef __linux__
    m_messages.back() += std::uint32_t(0x26F3);
    m_messages.back() += std::uint32_t(0x1F3CC);
#else
    m_messages.back() += std::uint32_t(0x1F3CC);
    m_messages.back() += std::uint32_t(0x26F3);
#endif

    static constexpr float TextDepth = 0.2f;
    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

    //root node for position
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { 0.5f, 0.5f };
    auto rootNode = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(m_messages[0]);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setVerticalSpacing(4.f);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
    entity.getComponent<cro::UIElement>().characterSize = UITextSize;
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_messageNode = entity;
    centreText();


    //continue message
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { 0.5f, 0.1f };
    rootNode = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Press Any Button To Continue");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
    entity.getComponent<cro::UIElement>().characterSize = InfoTextSize;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            static constexpr float HideTime = 3.f;
            auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
            ct += dt;
            if (ct > HideTime)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::Callback>().active = false;
                ct = 0.f;
            }
        };

    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_continueNode = entity;


    const auto camCallback = [](cro::Camera& cam)
        {
            const auto size = glm::vec2(cro::App::getWindow().getSize());
            cam.setOrthographic(0.f, size.x, 0.f, size.y, -10.f, 10.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    camCallback(cam);
    cam.resizeCallback = camCallback;
}

void ClubInfoState::centreText()
{
    auto bounds = cro::Text::getLocalBounds(m_messageNode);
    const glm::vec2 pos = { 0.f, std::round(bounds.height / 2.f) };
    m_messageNode.getComponent<cro::UIElement>().absolutePosition = pos;
    m_messageNode.getComponent<cro::Transform>().setPosition(pos);
}