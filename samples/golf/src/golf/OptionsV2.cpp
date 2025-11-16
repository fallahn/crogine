/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "InputBinding.hpp"
#include "MessageIDs.hpp"
#include "OptionsV2.hpp"
#include "SharedStateData.hpp"

#include <crogine/graphics/SpriteSheet.hpp>

namespace
{

}

OptionsV2::OptionsV2(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_showOptions       (false),
    m_animationTarget   (0.f),
    m_animationProgress (0.f),
    m_itemActive        (false),
    m_closeModal        (false),
    m_prevFocus         (0),
    m_prevHovered       (0)
{
    registerWindow(std::bind(&OptionsV2::optionsWindow, this));
    
    //TODO we don't really need this double init any more
    m_flagPreview.init(m_sharedData.flagPath);
    m_flagPreview.setText(m_sharedData.flagText);

    //convert the sprites to nav icons
    cro::SpriteSheet spriteSheet;
    if (spriteSheet.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_textureResource))
    {
        m_navTexture = spriteSheet.getTexture()->getGLHandle();

        const auto convertSprite =
            [&](const std::string& sprName)
            {
                const auto sprite = spriteSheet.getSprite(sprName);
                auto rect = sprite.getTextureRect();

                NavIcon ret;
                ret.size = { rect.width, rect.height };

                rect = sprite.getTextureRectNormalised();
                ret.uv0.x = rect.left;
                ret.uv0.y = rect.bottom + rect.height; //UV is flipped vertically

                ret.uv1.x = rect.left + rect.width;
                ret.uv1.y = rect.bottom;
                return ret;
            };
        m_navIcons[NavIcon::PSNext] = convertSprite("next_tab_ps");
        m_navIcons[NavIcon::PSPrev] = convertSprite("prev_tab_ps");
        m_navIcons[NavIcon::XBNext] = convertSprite("next_tab_xbox");
        m_navIcons[NavIcon::XBPrev] = convertSprite("prev_tab_xbox");
    }
}

//public
bool OptionsV2::handleEvent(const cro::Event& evt)
{
    if (m_showOptions)
    {
        const auto prevTab =
            [&]()
            {
                m_navigationContext.tabIndex = (m_navigationContext.tabIndex + (NavigationContext::TabID::Count - 1)) % NavigationContext::TabID::Count;
                m_navigationContext.requestedTab = m_navigationContext.tabIndex;
                playSound(MenuSoundEvent::Cancel); //nice intuitive naming...
            };

        const auto nextTab =
            [&]()
            {
                m_navigationContext.tabIndex = (m_navigationContext.tabIndex + 1) % NavigationContext::TabID::Count;
                m_navigationContext.requestedTab = m_navigationContext.tabIndex;
                playSound(MenuSoundEvent::Activate);
            };

        const auto setActiveInput =
            [&]()
            {
                if (evt.type == SDL_MOUSEBUTTONDOWN
                    || evt.type == SDL_MOUSEMOTION
                    || evt.type == SDL_KEYDOWN)
                {
                    //if mouse motion or key down
                    m_sharedData.activeInput = SharedStateData::ActiveInput::Keyboard;
                }
                else if (evt.type == SDL_CONTROLLERAXISMOTION
                    || evt.type == SDL_CONTROLLERBUTTONDOWN)
                {
                    //else controller axis or button
                    m_sharedData.activeInput = cro::GameController::hasPSLayout(cro::GameController::controllerID(evt.cbutton.which)) ?
                        SharedStateData::ActiveInput::PS : SharedStateData::ActiveInput::XBox;
                }
            };

        switch (evt.type)
        {
        default: break;
        case SDL_MOUSEBUTTONDOWN:
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                closeWindow();
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonB:
                closeWindow();
                break;
            case cro::GameController::ButtonLeftShoulder:
                prevTab();
                break;
            case cro::GameController::ButtonRightShoulder:
                nextTab();
                break;
            }
            setActiveInput();
            break;
        case SDL_KEYDOWN:
            switch (evt.key.keysym.sym)
            {
            default: break;
                case SDLK_ESCAPE:
                case SDLK_BACKSPACE:
                    closeWindow();
                    break;
            }

            if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                prevTab();
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
            {
                nextTab();
            }
            setActiveInput();
            break;
        case SDL_MOUSEMOTION:
            cro::App::getWindow().setMouseCaptured(false);
            setActiveInput();
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (evt.caxis.value > cro::GameController::LeftThumbDeadZone
                || evt.caxis.value < -cro::GameController::LeftThumbDeadZone)
            {
                cro::App::getWindow().setMouseCaptured(true);
                setActiveInput();
            }
            break;
        }
    }

    return false;
}

void OptionsV2::handleMessage(const cro::Message&) {}

bool OptionsV2::simulate(float dt)
{
    const float animSpeed = dt * 4.f;
    if (m_animationTarget < m_animationProgress)
    {
        m_animationProgress = std::max(0.f, m_animationProgress - animSpeed);
        if (m_animationProgress == 0)
        {
            requestStackPop();
        }
    }
    else if (m_animationTarget > m_animationProgress)
    {
        m_animationProgress = std::min(1.f, m_animationProgress + animSpeed);
    }

    return true;
}

void OptionsV2::render() {}


//private
void OptionsV2::onCachedPush()
{
    m_showOptions = true;
    m_animationTarget = 1.f;
}

void OptionsV2::onCachedPop()
{
    m_showOptions = false;
}

void OptionsV2::closeWindow()
{
    if (!m_itemActive)
    {
        m_animationTarget = 0.f;
        playSound(MenuSoundEvent::Cancel);
    }
    else
    {
        m_closeModal = true;
    }
}

void OptionsV2::playSound(std::int32_t s)
{
    auto* msg = postMessage<MenuSoundEvent>(cl::MessageID::MenuSoundMessage);
    msg->type = static_cast<std::uint8_t>(s);
}

void OptionsV2::checkbox(const char* s, bool* b)
{
    if (ImGui::Checkbox(s, b))
    {
        playSound(MenuSoundEvent::Activate);
    }
}