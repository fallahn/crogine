/*-----------------------------------------------------------------------

Matt Marchant 2026
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/core/OSK.hpp>
#include <crogine/core/Utf.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/graphics/Colour.hpp>

#include "../detail/TextConstruction.hpp"

using namespace cro;

namespace
{
    constexpr float VerticalKeyboardProportion = 3.f; //WindowSize is divided by this to create height of keyboard

    constexpr Colour BGColour = Colour(std::uint8_t(35), 38, 46);
    constexpr Colour ButtonColourNormal = Colour(std::uint8_t(14), 20, 27);
    constexpr Colour ButtonColourActive = Colour(std::uint8_t(255), 255, 255);
    constexpr Colour SpecialButtonNormal = Colour(std::uint8_t(0), 0, 0);
    constexpr Colour SpecialButtonActive = Colour(std::uint8_t(26), 159, 255);
    constexpr Colour TextColourNormal = ButtonColourActive;
    constexpr Colour TextColourActive = ButtonColourNormal;
    constexpr Colour TextColourShift = Colour(std::uint8_t(123), 126, 130); //shifted text icons EG number row when shift not active

    constexpr std::uint32_t ButtonRows = 5;
    constexpr std::uint32_t ButtonCols = 14;
    constexpr float ButtonWidth = 1.f / ButtonCols;

    //we assign a scancode to each of the virtual keys
    //that way we automatically display the correct keys
    //based on the user's layout as well as render different
    //characters is shift is locked.
    struct KeyInfo final
    {
        constexpr KeyInfo() {};
        constexpr KeyInfo(SDL_Scancode c, float w = ButtonWidth) : scancode(c), size(w) {}
        SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
        const float size = ButtonWidth; //percentage of available width
        bool active = false;
    };

    constexpr std::array<std::array<KeyInfo, ButtonCols>, ButtonRows> ButtonInfo =
    {
        std::array<KeyInfo, 14u>{KeyInfo(SDL_SCANCODE_GRAVE, ButtonWidth / 2.f),KeyInfo(SDL_SCANCODE_1),KeyInfo(SDL_SCANCODE_2),KeyInfo(SDL_SCANCODE_3),KeyInfo(SDL_SCANCODE_4),KeyInfo(SDL_SCANCODE_5),KeyInfo(SDL_SCANCODE_6),KeyInfo(SDL_SCANCODE_7),KeyInfo(SDL_SCANCODE_8),KeyInfo(SDL_SCANCODE_9),KeyInfo(SDL_SCANCODE_0),KeyInfo(SDL_SCANCODE_MINUS),KeyInfo(SDL_SCANCODE_EQUALS),KeyInfo(SDL_SCANCODE_BACKSPACE, ButtonWidth + (ButtonWidth / 2.f))},
        {KeyInfo(SDL_SCANCODE_TAB),KeyInfo(SDL_SCANCODE_Q),KeyInfo(SDL_SCANCODE_W),KeyInfo(SDL_SCANCODE_E),KeyInfo(SDL_SCANCODE_R),KeyInfo(SDL_SCANCODE_T),KeyInfo(SDL_SCANCODE_Y),KeyInfo(SDL_SCANCODE_U),KeyInfo(SDL_SCANCODE_I),KeyInfo(SDL_SCANCODE_O),KeyInfo(SDL_SCANCODE_P),KeyInfo(SDL_SCANCODE_LEFTBRACKET),KeyInfo(SDL_SCANCODE_RIGHTBRACKET),KeyInfo(SDL_SCANCODE_BACKSLASH)},
        {KeyInfo(SDL_SCANCODE_CAPSLOCK, ButtonWidth + (ButtonWidth / 2.f)),KeyInfo(SDL_SCANCODE_A),KeyInfo(SDL_SCANCODE_S),KeyInfo(SDL_SCANCODE_D),KeyInfo(SDL_SCANCODE_F),KeyInfo(SDL_SCANCODE_G),KeyInfo(SDL_SCANCODE_H),KeyInfo(SDL_SCANCODE_J),KeyInfo(SDL_SCANCODE_K),KeyInfo(SDL_SCANCODE_L),KeyInfo(SDL_SCANCODE_SEMICOLON),KeyInfo(SDL_SCANCODE_APOSTROPHE),KeyInfo(SDL_SCANCODE_RETURN, ButtonWidth + (ButtonWidth / 2.f)),KeyInfo()},
        {KeyInfo(SDL_SCANCODE_LSHIFT, ButtonWidth * 2.f),KeyInfo(SDL_SCANCODE_Z),KeyInfo(SDL_SCANCODE_X),KeyInfo(SDL_SCANCODE_C),KeyInfo(SDL_SCANCODE_V),KeyInfo(SDL_SCANCODE_B),KeyInfo(SDL_SCANCODE_N),KeyInfo(SDL_SCANCODE_M),KeyInfo(SDL_SCANCODE_COMMA),KeyInfo(SDL_SCANCODE_PERIOD),KeyInfo(SDL_SCANCODE_SLASH),KeyInfo(SDL_SCANCODE_RSHIFT, ButtonWidth * 2.f),KeyInfo(),KeyInfo()},
        {KeyInfo(SDL_SCANCODE_LEFT),KeyInfo(SDL_SCANCODE_SPACE, ButtonWidth * 12.f),KeyInfo(/*would be switch to emoji*/),KeyInfo(/*would be Paste*/),KeyInfo(/*would be quit*/),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(SDL_SCANCODE_RIGHT)}
    };

    std::array<std::array<FloatRect, ButtonCols>, ButtonRows> Hitboxes = {};

    //TODO do we need these or can the output of scancodes be inferred by shift status?
    //constexpr std::array<std::array<KeyInfo, 14u>, 5u> UpperCaseKeys =
    //{
    //    std::array<KeyInfo, 14u>{KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()},
    //    {KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()},
    //    {KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()},
    //    {KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()},
    //    {KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()}
    //};

    //xbox
    static constexpr inline std::uint32_t ButtonLT = 0x2196;
    static constexpr inline std::uint32_t ButtonRT = 0x2197;
    static constexpr inline std::uint32_t ButtonLB = 0x2198;
    static constexpr inline std::uint32_t ButtonRB = 0x2199;
    static constexpr inline std::uint32_t ButtonX = 0x21D0;
    static constexpr inline std::uint32_t ButtonY = 0x21D1;
    static constexpr inline std::uint32_t ButtonB = 0x21D2;
    static constexpr inline std::uint32_t ButtonA = 0x21D3;
    static constexpr inline std::uint32_t ButtonStart = 0x21FB;


    //ps
    static constexpr inline std::uint32_t ButtonL1 = 0x21B0;
    static constexpr inline std::uint32_t ButtonR1 = 0x21B1;
    static constexpr inline std::uint32_t ButtonL2 = 0x21B2;
    static constexpr inline std::uint32_t ButtonR2 = 0x21B3;
    static constexpr inline std::uint32_t ButtonSquare = 0x21E0;
    static constexpr inline std::uint32_t ButtonTriangle = 0x21E1;
    static constexpr inline std::uint32_t ButtonCircle = 0x21E2;
    static constexpr inline std::uint32_t ButtonCross = 0x21E3;
    static constexpr inline std::uint32_t ButtonOption = 0x21E8;


    static constexpr inline std::uint32_t LeftStick = 0x21EF;
    static constexpr inline std::uint32_t RightStick = 0x21C6;



    //keyboard
    constexpr std::uint32_t IconLeft = 0x23F4;
    constexpr std::uint32_t IconRight = 0x23F5;
    constexpr std::uint32_t IconUp = 0x23F6;
    constexpr std::uint32_t IconDown = 0x23F7;
    constexpr std::uint32_t IconShift = 0x2429;
    constexpr std::uint32_t IconTab = 0x242B; //TODO this won't actually do anything - maybe emoji switch?
    constexpr std::uint32_t IconCaps = 0x242C;
    constexpr std::uint32_t IconBackspace = 0x242D;
    constexpr std::uint32_t IconReturn = 0x242E;
    constexpr std::uint32_t IconSpace = 0x243A;


    constexpr std::uint32_t BasePreviewTextSize = 12; //gets scaled based on screen size
    constexpr std::uint32_t BaseKeyTextSize = 8; //as above
}

//public
OSK::OSK()
    : m_bufferIndex     (0),
    m_rowIndex          (0),
    m_colIndex          (0),
    m_keymod            (0),
    m_controllerMask    (0),
    m_prevControllerMask(0),
    m_isActive          (false)
{
    m_keyboardArray.setPrimitiveType(GL_TRIANGLES);
    m_keyTextArray.setPrimitiveType(GL_TRIANGLES);
    m_xboxIcons.setPrimitiveType(GL_TRIANGLES);
    m_keyIcons.setPrimitiveType(GL_TRIANGLES);
    m_PSIcons.setPrimitiveType(GL_TRIANGLES);

    if (m_textFont.loadFromFile(std::filesystem::path("assets/golf/fonts/NotoSans-Regular.ttf")))
    {
        //append other fonts such as CJK - TODO these should really be in the assets root folder
        FontAppendmentContext ctx;
        static const std::array FontMappings =
        {
            //std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", CodePointRange::Cyrillic),
            //std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", CodePointRange::Greek),
            //std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", std::array<std::uint32_t, 2u>({0x0100, 0x017F})), //extended latin-a
            //std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", std::array<std::uint32_t, 2u>({0x0180, 0x024F})), //extended latin-b
            std::make_pair("assets/golf/fonts/NotoSansThai-Regular.ttf", std::array<std::uint32_t, 2u>({0x2010, 0x205E})),
            std::make_pair("assets/golf/fonts/NotoSansThai-Regular.ttf", std::array<std::uint32_t, 2u>({0x0E00, 0x0E7F})),
            std::make_pair("assets/golf/fonts/NotoSansKR-Regular.ttf", std::array<std::uint32_t, 2u>({0x3131, 0x3163})),
            std::make_pair("assets/golf/fonts/NotoSansKR-Regular.ttf", std::array<std::uint32_t, 2u>({0xAC00, 0xD7A3})),
            std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0x2000, 0x206F})),
            std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0x3000, 0x30FF})),
            std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0x31F0, 0x31FF})),
            std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0xFF00, 0xFFEF})),
            std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0x4e00, 0x9FAF})),
        };

        for (const auto& [path, codepoints] : FontMappings)
        {
            if (FileSystem::fileExists(path))
            {
                ctx.codepointRange = codepoints;
                m_textFont.appendFromFile(path, ctx);
            }
        }

        //controller icon font
        ctx.codepointRange = {0x2190,0x21FF}; //controller input
        m_textFont.appendFromFile("assets/fonts/promptfont.ttf", ctx);
        ctx.codepointRange = {0x23F4,0x242E}; //keyboard icons, shift etc
        m_textFont.appendFromFile("assets/fonts/promptfont.ttf", ctx);

        //TODO add emoji font if we allow for that input


        //emoji fonts
//        ctx.allowBold = false;
//        ctx.allowFillColour = false;
//        ctx.allowOutline = false;
//
//        static constexpr std::array Ranges =
//        {
//            CodePointRange::EmojiLower,
//            CodePointRange::EmojiMid,
//            CodePointRange::EmojiUpper,
//        };
//
//#ifdef _WIN32
//        const std::string winPath = "C:/Windows/Fonts/seguiemj.ttf";
//
//        if (FileSystem::fileExists(winPath))
//        {
//            for (const auto& r : Ranges)
//            {
//                ctx.codepointRange = r;
//                m_textFont.appendFromFile(winPath, ctx);
//            }
//        }
//        else
//#endif
//        {
//            const std::string path = "assets/golf/fonts/TwemojiCOLRv0.ttf";
//
//            for (const auto& r : Ranges)
//            {
//                ctx.codepointRange = r;
//                m_textFont.appendFromFile(path, ctx);
//            }
//        }


        //prime the font with some texture pages to prevent GL errors
        //when first assigned to the key text array
        for (auto i = 1; i < static_cast<std::int32_t>(UIElementSystem::MaxViewScale) + 1; ++i) //each possible window scale
        {
            m_textFont.getGlyph(97, BasePreviewTextSize * i);
            m_textFont.getGlyph(97, BaseKeyTextSize * i);
        }


        m_previewText.setFillColour(Colour::Black);
        m_previewText.setFont(m_textFont);
        m_previewText.setAlignment(SimpleText::Alignment::Centre);
        

        //TODO we need to register this class to the font callback so that it can
        //tell the text to rebuild if necessary. Probably. It gets rebuilt a lot...
    }
}

void OSK::show(const std::function<void(bool, const char*)>& callback)
{
    auto& instance = App::getInstance().m_osk;

    if (!instance->m_isActive)
    {
        instance->m_isActive = true;
        instance->m_callback = callback;
        instance->m_bufferIndex = 0;

        instance->updateVertices();
    }
}


//private
void OSK::close(bool isSubmitted)
{
    if (m_isActive)
    {
        if (m_callback)
        {
            std::basic_string<char> output;
            output.reserve(m_textBuffer.size());
            Utf32::toUtf8(m_textBuffer.begin(), m_textBuffer.end(), std::back_inserter(output));

            m_callback(isSubmitted, output.data());
        }

        //reset the buffer for next input
        //if (isSubmitted)
        {
            std::fill(std::begin(m_textBuffer), std::end(m_textBuffer), 0);
            m_bufferIndex = 0;

            m_previewText.setString(" ");
        }

        m_isActive = false;
    }
}

void OSK::updateVertices()
{
    const auto WindowSize = glm::vec2(App::getWindow().getSize());
    std::vector<Vertex2D> verts;

    //background
    const float BGHeight = std::floor(WindowSize.y / VerticalKeyboardProportion);
    verts.emplace_back(glm::vec2(0.f, BGHeight), BGColour);
    verts.emplace_back(glm::vec2(0.f), BGColour);
    verts.emplace_back(glm::vec2(WindowSize.x, BGHeight), BGColour);

    verts.emplace_back(glm::vec2(WindowSize.x, BGHeight), BGColour);
    verts.emplace_back(glm::vec2(0.f), BGColour);
    verts.emplace_back(glm::vec2(WindowSize.x, 0.f), BGColour);

    static constexpr float DefaultPadding = 2.f;
    const auto Scale = UIElementSystem::getViewScale();
    
    //calc padding and char size based on scale
    const auto Padding = DefaultPadding * Scale;

    //total text area width should be 4:3 ratio of the current window height
    auto keyWidth = (WindowSize.y / 3.f) * 4.f;
    keyWidth = std::min(keyWidth, WindowSize.x);
    keyWidth -= Padding;
    keyWidth = std::floor(keyWidth);

    const auto keyHeight = std::floor(BGHeight - Padding);
    const auto keySize = glm::vec2(std::floor(keyWidth / ButtonCols), std::floor(keyHeight / ButtonRows)) - glm::vec2(Padding);

    const auto startX = ((WindowSize.x - keyWidth) / 2.f) + Padding;
    const auto startY = BGHeight - keySize.y - Padding;

    //track the centre pos so we dont have to recalc when placing text
    struct KeyText final
    {
        cro::String label;
        glm::vec2 centre = glm::vec2(0.f);
        glm::vec2 corner = glm::vec2(0.f);
        SDL_Keycode key = 0;
        SDL_Keycode keyXB = 0;
        SDL_Keycode keyPS = 0;
    };
    std::vector<std::vector<KeyText>> centrePos;

    //NOTE key size x is actually keyWidth * Button.size - Padding
    float y = startY;
    for (auto j = 0u; j < ButtonRows; ++j)
    {
        float x = startX;
        auto& centres = centrePos.emplace_back();
        for (auto i = 0u; i < ButtonCols; ++i)
        {
            const auto& buttonInf = ButtonInfo[j][i];
            const auto buttonWidth = std::round((keyWidth * buttonInf.size) - Padding);
            auto k = SDL_GetKeyFromScancode(buttonInf.scancode, m_keymod, false);
            
            if (buttonInf.scancode != SDL_SCANCODE_UNKNOWN)
            {
                auto c = ButtonColourNormal;
                std::uint32_t xb = 0;
                std::uint32_t ps = 0;
                cro::String label;

                if (m_rowIndex == j && m_colIndex == i)
                {
                    c = ButtonColourActive;
                }                
                /*else*/ if (((k & (SDLK_EXTENDED_MASK | SDLK_SCANCODE_MASK)) != 0)
                    || k == SDLK_TAB || k == SDLK_RETURN || k == SDLK_BACKSPACE || k == SDLK_SPACE)
                {
                    //set to blue if this is a shift and mod mode is not none
                    if ((m_keymod & SDL_KMOD_SHIFT)
                        && (buttonInf.scancode == SDL_SCANCODE_LSHIFT || buttonInf.scancode == SDL_SCANCODE_RSHIFT))
                    {
                        c = SpecialButtonActive;
                    }
                    else
                    {
                        if (c == ButtonColourNormal)
                        {
                            c = SpecialButtonNormal;
                        }
                    }
                    
                    switch (buttonInf.scancode)
                    {
                    default: 
                        k = 0;
                        break;
                    case SDL_SCANCODE_TAB:
                        k = IconTab;
                        label = "Tab";
                        break;
                    case SDL_SCANCODE_CAPSLOCK:
                        k = IconCaps;
                        xb = ps = LeftStick;
                        label = "CAPS";
                        break;
                    case SDL_SCANCODE_LSHIFT:
                    case SDL_SCANCODE_RSHIFT:
                        k = IconShift;
                        xb = ButtonLT;
                        ps = ButtonL2;
                        label = "Shift";
                        break;
                    case SDL_SCANCODE_RETURN:
                        k = IconReturn;
                        xb = ButtonRT;
                        ps = ButtonR2;
                        label = "Enter";
                        break;
                    case SDL_SCANCODE_BACKSPACE:
                        k = IconBackspace;
                        xb = ButtonX;
                        ps = ButtonSquare;
                        label = "BackSP";
                        break;
                    case SDL_SCANCODE_SPACE:
                        k = IconSpace;
                        xb = ButtonY;
                        ps = ButtonTriangle;
                        break;
                    }
                }

                //clamps the width so rounding error at makes
                //sure the far edges of buttons line up
                const float farX = std::min(x + buttonWidth, keyWidth + startX - (Padding * 2.f));

                verts.emplace_back(glm::vec2(x, y + keySize.y), c);
                verts.emplace_back(glm::vec2(x, y), c);
                verts.emplace_back(glm::vec2(farX, y + keySize.y), c);
                verts.emplace_back(glm::vec2(farX, y + keySize.y), c);
                verts.emplace_back(glm::vec2(x, y), c);
                verts.emplace_back(glm::vec2(farX, y), c);
                
                Hitboxes[j][i] = FloatRect(x, y, buttonWidth, keySize.y);

                centres.emplace_back().centre = { std::round(x + (buttonWidth / 2.f)), std::round(y + (keySize.y / 2.f)) };
                centres.back().corner = { x + (2.f * Scale), std::round(y + (keySize.y / 2.f)) };
                centres.back().key = k;
                centres.back().keyXB = xb;
                centres.back().keyPS = ps;
                centres.back().label = label;

                x += (buttonWidth + Padding);
            }
            else
            {
                Hitboxes[j][i] = FloatRect(0.f, 0.f, 0.f, 0.f);
                centres.emplace_back().key = 0;
            }
        }
        y -= (keySize.y + Padding);
    }

    static constexpr float InputHeight = 22.f;
    verts.emplace_back(glm::vec2(0.f, BGHeight + (InputHeight * Scale)), SpecialButtonNormal);
    verts.emplace_back(glm::vec2(0.f, BGHeight), SpecialButtonNormal);
    verts.emplace_back(glm::vec2(WindowSize.x, BGHeight + (InputHeight * Scale)), SpecialButtonNormal);
    verts.emplace_back(glm::vec2(WindowSize.x, BGHeight + (InputHeight * Scale)), SpecialButtonNormal);
    verts.emplace_back(glm::vec2(0.f, BGHeight), SpecialButtonNormal);
    verts.emplace_back(glm::vec2(WindowSize.x, BGHeight), SpecialButtonNormal);

    //TODO place proper vertices for a border so we don't have overdraw
    verts.emplace_back(glm::vec2(Padding, BGHeight + (InputHeight * Scale) - Padding), ButtonColourActive);
    verts.emplace_back(glm::vec2(Padding, BGHeight + Padding), ButtonColourActive);
    verts.emplace_back(glm::vec2(WindowSize.x - Padding, BGHeight + (InputHeight * Scale) - Padding), ButtonColourActive);
    verts.emplace_back(glm::vec2(WindowSize.x - Padding, BGHeight + (InputHeight * Scale) - Padding), ButtonColourActive);
    verts.emplace_back(glm::vec2(Padding, BGHeight + Padding), ButtonColourActive);
    verts.emplace_back(glm::vec2(WindowSize.x- Padding, BGHeight + Padding), ButtonColourActive);

    m_keyboardArray.setVertexData(verts);


    const auto keyTextSize = BaseKeyTextSize * static_cast<std::uint32_t>(Scale);
    const auto iconTextSize = keyTextSize * 3;
    verts.clear();

    std::vector<Vertex2D> xbVerts;
    std::vector<Vertex2D> psVerts;
    std::vector<Vertex2D> kbVerts;

    //calc text size and fetch glyphs
    for (auto j = 0u; j < ButtonRows; ++j)
    {
        for (auto i = 0u; i < ButtonCols; ++i)
        {
            const auto& [label, centre, corner, key, keyXB, keyPS] = centrePos[j][i];
            const auto c = (j == m_rowIndex && i == m_colIndex) ? Colour::Black : Colour::White;

            switch (key)
            {
            case 0: break;
            default:
            {
                const auto glyph = m_textFont.getGlyph(key, keyTextSize);
                Detail::Text::addQuad(verts, centre - glm::vec2(glyph.bounds.width / 2.f, glyph.bounds.height / 2.f), 
                                    c, glyph, m_textFont.getTexture(keyTextSize).getSize());
            }
                break;
            case IconShift:
            case IconSpace:
            case IconCaps:
            case IconBackspace:
            case IconReturn:
            case IconUp:
            case IconDown:
            case IconLeft:
            case IconRight:
            {
                //we need to find the specific codepoints for each input icon
                auto glyph = m_textFont.getGlyph(key, iconTextSize);
                Detail::Text::addQuad(kbVerts, corner - glm::vec2(0.f, glyph.bounds.height / 2.f),
                                    c, glyph, m_textFont.getTexture(iconTextSize).getSize());

                glyph = m_textFont.getGlyph(keyXB, iconTextSize);
                Detail::Text::addQuad(xbVerts, corner - glm::vec2(0.f, glyph.bounds.height / 2.f),
                    c, glyph, m_textFont.getTexture(iconTextSize).getSize());

                glyph = m_textFont.getGlyph(keyPS, iconTextSize);
                Detail::Text::addQuad(psVerts, corner - glm::vec2(0.f, glyph.bounds.height / 2.f),
                    c, glyph, m_textFont.getTexture(iconTextSize).getSize());

                if (!label.empty())
                {
                    float x = 0.f;

                    std::uint32_t prevChar = 0;
                    for (auto m = 0u; m < label.size(); ++m)
                    {
                        std::uint32_t currChar = label[m];
                        //TODO figure out why this returns 0
                        x += keyTextSize; //m_textFont.getKerning(prevChar, currChar, keyTextSize);
                        prevChar = currChar;

                        glyph = m_textFont.getGlyph(currChar, keyTextSize);
                        auto pos = corner - glm::vec2(-static_cast<std::int32_t>(iconTextSize), glyph.bounds.height / 2.f);
                        pos.x += x;
                        Detail::Text::addQuad(verts, pos, c, glyph, m_textFont.getTexture(keyTextSize).getSize());
                    }
                }
            }
                break;
            }
        }
    }

    //texture may have updated so always reassign
    m_keyTextArray.setTexture(m_textFont.getTexture(keyTextSize));
    //TODO how do we do text that's more than one char?
    m_keyTextArray.setVertexData(verts);

    m_xboxIcons.setTexture(m_textFont.getTexture(iconTextSize));
    m_xboxIcons.setVertexData(xbVerts);

    m_keyIcons.setTexture(m_textFont.getTexture(iconTextSize));
    m_keyIcons.setVertexData(kbVerts);

    m_PSIcons.setTexture(m_textFont.getTexture(iconTextSize));
    m_PSIcons.setVertexData(psVerts);


    //move this somewhere sensible
    const auto charSize = BasePreviewTextSize * static_cast<std::uint32_t>(Scale);
    m_previewText.setCharacterSize(charSize);
    m_previewText.setPosition({ std::floor(WindowSize.x / 2.f), std::floor(BGHeight + (charSize / 2)) });
}

bool OSK::keypress(SDL_Scancode code)
{
    if (code == SDL_SCANCODE_CAPSLOCK)
    {
        m_keymod = m_keymod == SDL_KMOD_NONE ? SDL_KMOD_SHIFT : SDL_KMOD_NONE;
        updateVertices();
        return true;
    }

    const auto k = SDL_GetKeyFromScancode(code, m_keymod, false);
    if ((k & (SDLK_EXTENDED_MASK | SDLK_SCANCODE_MASK)) == 0)
    {
        switch (k)
        {
        default:
            if (m_bufferIndex < MaxChars)
            {
                m_textBuffer[m_bufferIndex++] = k;
                m_previewText.setString(String::fromUtf32(m_textBuffer.begin(), m_textBuffer.begin() + m_bufferIndex));

                return true;
            }
            return false;
        case SDLK_BACKSPACE:
            if (m_bufferIndex > 0)
            {
                m_textBuffer[--m_bufferIndex] = 0;
                m_previewText.setString(String::fromUtf32(m_textBuffer.begin(), m_textBuffer.begin() + m_bufferIndex));
                return true;
            }
            return false;

        case SDLK_RETURN:
        case SDLK_RETURN2:
            close(true);
            return false;
        }
    }
    return false;
}

void OSK::moveLeft()
{
    do
    {
        m_colIndex = (m_colIndex + (ButtonCols - 1)) % ButtonCols;
    } while (ButtonInfo[m_rowIndex][m_colIndex].scancode == SDL_SCANCODE_UNKNOWN);

    updateVertices();
}

void OSK::moveRight()
{
    do
    {
        m_colIndex = (m_colIndex + 1) % ButtonCols;
    } while (ButtonInfo[m_rowIndex][m_colIndex].scancode == SDL_SCANCODE_UNKNOWN);

    updateVertices();
}

void OSK::moveUp()
{
    //moving up/down is fine, but we need
    //to correct the left/right if we land
    //on an invalid slot
    m_rowIndex = (m_rowIndex + (ButtonRows - 1)) % ButtonRows;

    while (ButtonInfo[m_rowIndex][m_colIndex].scancode == SDL_SCANCODE_UNKNOWN)
    {
        m_colIndex = (m_colIndex + (ButtonCols - 1)) % ButtonCols;
    }

    updateVertices();
}

void OSK::moveDown()
{
    m_rowIndex = (m_rowIndex + 1) % ButtonRows;

    while (ButtonInfo[m_rowIndex][m_colIndex].scancode == SDL_SCANCODE_UNKNOWN)
    {
        m_colIndex = (m_colIndex + (ButtonCols - 1)) % ButtonCols;
    }

    updateVertices();
}

void OSK::mouseClick(glm::vec2 mousePos)
{
    //hmmm we're not using a camera - what do we use to convert coords?
    //we can probably just make assumptions as we're rendering at window scale
    const auto WindowSize = glm::vec2(App::getWindow().getSize());
    const auto pos = glm::vec2(mousePos.x, WindowSize.y - mousePos.y);
    
    if (pos.y < std::floor(WindowSize.y / VerticalKeyboardProportion))
    {
        for (auto j = 0u; j < ButtonRows; ++j)
        {
            for (auto i = 0u; i < ButtonCols; ++i)
            {
                if (Hitboxes[j][i].contains(pos))
                {
                    m_rowIndex = j;
                    m_colIndex = i;
                    keypress(ButtonInfo[j][i].scancode);
                    updateVertices();
                    return;
                }
            }
        }
        
    }
}

bool OSK::handleEvent(const Event& evt)
{
    if (!m_isActive)
    {
        return false;
    }

    const auto applyAxisMotion = 
        [this]()
        {
            const auto testBits = 
                [](std::uint8_t mask, std::uint8_t bits)
                {
                    return (mask & bits) != 0;
                };

            if (m_prevControllerMask != m_controllerMask)
            {
                if (!testBits(m_prevControllerMask, ControllerBits::Left)
                    && testBits(m_controllerMask, ControllerBits::Left))
                {
                    moveLeft();
                }
                if (!testBits(m_prevControllerMask, ControllerBits::Right)
                    && testBits(m_controllerMask, ControllerBits::Right))
                {
                    moveRight();
                }
                if (!testBits(m_prevControllerMask, ControllerBits::Up)
                    && testBits(m_controllerMask, ControllerBits::Up))
                {
                    moveUp();
                }
                if (!testBits(m_prevControllerMask, ControllerBits::Down)
                    && testBits(m_controllerMask, ControllerBits::Down))
                {
                    moveDown();
                }
                if (!testBits(m_prevControllerMask, ControllerBits::L2)
                    && testBits(m_controllerMask, ControllerBits::L2))
                {
                    m_keymod = SDL_KMOD_SHIFT;
                    updateVertices();
                }
                else if (testBits(m_prevControllerMask, ControllerBits::L2)
                    && !testBits(m_controllerMask, ControllerBits::L2))
                {
                    m_keymod = SDL_KMOD_NONE;
                    updateVertices();
                }

                if (!testBits(m_prevControllerMask, ControllerBits::R2)
                    && testBits(m_controllerMask, ControllerBits::R2))
                {
                    keypress(SDL_SCANCODE_RETURN);
                }
            }
            m_prevControllerMask = m_controllerMask;
        };

    struct IconSet final
    {
        enum
        {
            PS, XBox, Keys
        };
    };
    const auto applyIconSet =
        [this](std::int32_t set)
        {
            switch (set)
            {
            default: 
            case IconSet::Keys:
                m_keyIcons.setScale(glm::vec2(1.f));
                m_PSIcons.setScale(glm::vec2(0.f));
                m_xboxIcons.setScale(glm::vec2(0.f));
                break;
            case IconSet::PS:
                m_keyIcons.setScale(glm::vec2(0.f));
                m_PSIcons.setScale(glm::vec2(1.f));
                m_xboxIcons.setScale(glm::vec2(0.f));
                break;
            case IconSet::XBox:
                m_keyIcons.setScale(glm::vec2(0.f));
                m_PSIcons.setScale(glm::vec2(0.f));
                m_xboxIcons.setScale(glm::vec2(1.f));
                break;
            }
        };

    switch (evt.type)
    {
    default: return false;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    {
        //thumbstick movement
        const std::int16_t Threshold = GameController::LeftThumbDeadZone * 2;// 15000;
        const std::int32_t icon = GameController::hasPSLayout(GameController::controllerID(evt.gaxis.which)) ?
            IconSet::PS : IconSet::XBox;
        switch (evt.gaxis.axis)
        {
        default: break;
        case SDL_GAMEPAD_AXIS_LEFTX:
            if (evt.gaxis.value > Threshold)
            {
                //right
                m_controllerMask |= ControllerBits::Right;
                m_controllerMask &= ~ControllerBits::Left;
                applyIconSet(icon);
            }
            else if (evt.gaxis.value < -Threshold)
            {
                //left
                m_controllerMask |= ControllerBits::Left;
                m_controllerMask &= ~ControllerBits::Right;
                applyIconSet(icon);
            }
            else
            {
                m_controllerMask &= ~(ControllerBits::Left | ControllerBits::Right);
            }
            applyAxisMotion();
            break;
        case SDL_GAMEPAD_AXIS_LEFTY:
            if (evt.gaxis.value > Threshold)
            {
                //down
                m_controllerMask |= ControllerBits::Down;
                m_controllerMask &= ~ControllerBits::Up;
                applyIconSet(icon);
            }
            else if (evt.gaxis.value < -Threshold)
            {
                //up
                m_controllerMask |= ControllerBits::Up;
                m_controllerMask &= ~ControllerBits::Down;
                applyIconSet(icon);
            }
            else
            {
                m_controllerMask &= ~(ControllerBits::Up | ControllerBits::Down);
            }
            applyAxisMotion();
            break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            if (evt.gaxis.value > GameController::TriggerDeadZone)
            {
                m_controllerMask |= ControllerBits::L2;
                applyIconSet(icon);
            }
            else if (evt.gaxis.value < GameController::TriggerDeadZone)
            {
                m_controllerMask &= ~ControllerBits::L2;
            }
            applyAxisMotion();
            break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
            if (evt.gaxis.value > GameController::TriggerDeadZone)
            {
                m_controllerMask |= ControllerBits::R2;
                applyIconSet(icon);
            }
            else if (evt.gaxis.value < GameController::TriggerDeadZone)
            {
                m_controllerMask &= ~ControllerBits::R2;
            }
            applyAxisMotion();
            break;
        }
    }
        return m_isActive; //I mean... the clause above should mean this is always true at this point...
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        switch (evt.gbutton.button)
        {
        default: break;
        case GameController::ButtonB:
            close(false);
            break;
        }
        return m_isActive;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    {
        const std::int32_t icon = GameController::hasPSLayout(GameController::controllerID(evt.gbutton.which)) ?
            IconSet::PS : IconSet::XBox;
        applyIconSet(icon);
    }
        switch (evt.gbutton.button)
        {
        default: break;
        case GameController::DPadLeft:
            moveLeft();
            break;
        case GameController::DPadRight:
            moveRight();
            break;
        case GameController::DPadUp:
            moveUp();
            break;
        case GameController::DPadDown:
            moveDown();
            break;
        case GameController::ButtonLeftStick:
            keypress(SDL_SCANCODE_CAPSLOCK);
            break;
        case GameController::ButtonA:
            keypress(ButtonInfo[m_rowIndex][m_colIndex].scancode);
            break;
        case GameController::ButtonX:
            keypress(SDL_SCANCODE_BACKSPACE);
            break;
        }

        return m_isActive;

    case SDL_EVENT_KEY_UP:
        switch (evt.key.key)
        {
        default: break;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            m_keymod = SDL_KMOD_NONE;
            updateVertices();
            break;
        case SDLK_ESCAPE:
            close(false);
            break;
        }
        return m_isActive;
    case SDL_EVENT_KEY_DOWN:
    {
        applyIconSet(IconSet::Keys);
        switch (evt.key.key)
        {
        default: break;;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            m_keymod = SDL_KMOD_SHIFT;
            updateVertices();
            return true;
        }
        keypress(evt.key.scancode);
    }
    return true;


    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            close(false);
        }
        return m_isActive;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            mouseClick({ evt.button.x, evt.button.y });
        }
        return m_isActive;
    case SDL_EVENT_MOUSE_MOTION:
        //TODO use mouse coords to set active index and refresh verts?
        //or we could just accept wherever the mouse click is
        applyIconSet(IconSet::Keys);
        return m_isActive;


    case SDL_EVENT_WINDOW_RESIZED:
        updateVertices();
        return false;
    }
}

void OSK::render()
{
    if (m_isActive)
    {
        m_keyboardArray.draw();
        m_keyTextArray.draw();

        //TODO skip drawing hidden
        //icons completely...
        m_xboxIcons.draw();
        m_PSIcons.draw();
        m_keyIcons.draw();

        m_previewText.draw();
    }
}