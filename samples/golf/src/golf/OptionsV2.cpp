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
    m_prevHovered       (0),
    m_rebindIndex       (-1)
{
    registerWindow(std::bind(&OptionsV2::optionsWindow, this));
    
    //TODO we don't really need this double init any more
    m_flagPreview.init(m_sharedData.flagPath);
    m_flagPreview.setText(m_sharedData.flagText);

    //convert the sprites to nav icons
    cro::SpriteSheet spriteSheet;
    const auto convertSprite =
        [&](const std::string& sprName)
        {
            const auto sprite = spriteSheet.getSprite(sprName);
            auto rect = sprite.getTextureRect();

            Icon ret;
            ret.size = { rect.width, rect.height };

            rect = sprite.getTextureRectNormalised();
            ret.uv0.x = rect.left;
            ret.uv0.y = rect.bottom + rect.height; //UV is flipped vertically

            ret.uv1.x = rect.left + rect.width;
            ret.uv1.y = rect.bottom;


            //this is a hack for the image buttons
            rect.bottom += (rect.height * 2.f);
            ret.uv2 = ret.uv0;
            ret.uv2.y = rect.bottom + rect.height;

            ret.uv3 = ret.uv1;
            ret.uv3.y = rect.bottom;

            return ret;
        };

    if (spriteSheet.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_textureResource))
    {
        m_navTexture = spriteSheet.getTexture()->getGLHandle();

        m_navIcons[NavIcon::PSNext] = convertSprite("next_tab_ps");
        m_navIcons[NavIcon::PSPrev] = convertSprite("prev_tab_ps");
        m_navIcons[NavIcon::XBNext] = convertSprite("next_tab_xbox");
        m_navIcons[NavIcon::XBPrev] = convertSprite("prev_tab_xbox");
    }

    if (spriteSheet.loadFromFile("assets/golf/sprites/options.spt", m_textureResource))
    {
        m_buttonTexture = spriteSheet.getTexture()->getGLHandle();

        m_buttonIcons[ButtonIcon::ResetHints] = convertSprite("reset_hints");
        m_buttonIcons[ButtonIcon::ResetCareer] = convertSprite("reset_career");
        m_buttonIcons[ButtonIcon::ResetProfile] = convertSprite("reset_button");
        m_buttonIcons[ButtonIcon::HowToPlay] = convertSprite("how_to_play");
        m_buttonIcons[ButtonIcon::Credits] = convertSprite("credits");
        m_buttonIcons[ButtonIcon::Close] = convertSprite("close");
    }

    if (spriteSheet.loadFromFile("assets/golf/sprites/control_layout.spt", m_textureResource))
    {
        m_controllerTexture = spriteSheet.getTexture()->getGLHandle();

        m_controllerIcons[ControllerIcon::Xbox] = convertSprite("xbox");
        m_controllerIcons[ControllerIcon::Deck] = convertSprite("deck");
        m_controllerIcons[ControllerIcon::PS] = convertSprite("ps");
    }
}

//public
bool OptionsV2::handleEvent(const cro::Event& evt)
{
    if (m_showOptions)
    {
        if (m_rebindIndex != -1)
        {
            //rebinding is active
            if (evt.type == SDL_KEYDOWN)
            {
                switch (evt.key.keysym.sym)
                {
                default: break;
                case SDLK_ESCAPE:
                case SDLK_BACKSPACE:
                    closeWindow();
                    break;
                }

                updateKeybind(evt.key.keysym.sym);
            }

            else if (evt.type == SDL_MOUSEBUTTONDOWN
                && evt.button.button == SDL_BUTTON_RIGHT)
            {
                closeWindow();
            }

            else if (evt.type == SDL_CONTROLLERBUTTONDOWN
                && evt.cbutton.button == cro::GameController::ButtonB)
            {
                closeWindow();
            }

            return false;
        }


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

            //hmm this breaks if the user assigns something like return to one of these
            /*if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                prevTab();
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
            {
                nextTab();
            }*/
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

void OptionsV2::confirmModal(const char* text, std::function<void()> cb, ImVec2 size, float scale)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::BeginChild("##child_modal", { -1.f, size.y - (40.f * scale) }, ImGuiChildFlags_NavFlattened);
    ImGui::Text("%s", text);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    const auto buttonWidth = ((size.x / 2.f) - (ImGui::GetStyle().ItemSpacing.x * 1.5f));
    if (ImGui::Button("Cancel", { buttonWidth, 0.f })
        || m_closeModal)
    {
        ImGui::CloseCurrentPopup();
        playSound(MenuSoundEvent::Cancel);
    }
    ImGui::SameLine();
    if (ImGui::Button("OK", { buttonWidth, 0.f }))
    {
        cb();
        ImGui::CloseCurrentPopup();
        playSound(MenuSoundEvent::Activate);
    }
    ImGui::EndPopup();

    m_itemActive = true;
}

void OptionsV2::updateKeybind(SDL_Keycode key)
{
    //prevent binding top row and function keys
    static constexpr std::array LockedKeys =
    {
        SDLK_1,
        SDLK_2,
        SDLK_3,
        SDLK_4,
        SDLK_5,
        SDLK_6,
        SDLK_7,
        SDLK_8,
        SDLK_9,
        SDLK_0,

        SDLK_F1,
        SDLK_F2,
        SDLK_F3,
        SDLK_F4,
        SDLK_F5,
        SDLK_F6,
        SDLK_F7,
        SDLK_F8,
        SDLK_F9,
        SDLK_F10,
        SDLK_F11,
        SDLK_F12,

        SDLK_KP_MINUS,
        SDLK_KP_PLUS,
        SDLK_TAB
    };
    if (const auto result = std::find(std::cbegin(LockedKeys), std::cend(LockedKeys), key);
        result != std::cend(LockedKeys))
    {
        playSound(MenuSoundEvent::Denied);
        m_rebindMessage = "This Key Cannot Be Assigned";
        return;
    }


    auto& keys = m_sharedData.inputBinding.keys;
    if (const auto result = std::find(keys.cbegin(), keys.cend(), key);
        result != keys.cend())
    {
        playSound(MenuSoundEvent::Denied);
        m_rebindMessage = "This Key Is Already Bound to:\n\n" + InputLabels[std::distance(keys.cbegin(), result)];
        return;
    }

    //these keys cancel the input
    if (key != SDLK_ESCAPE
        && key != SDLK_BACKSPACE)
    {
        keys[m_rebindIndex] = key;
        playSound(MenuSoundEvent::Activate);
    }
    else
    {
        playSound(MenuSoundEvent::Cancel);
    }

    closeWindow();
    m_rebindMessage.clear();
    m_rebindIndex = -1;
}