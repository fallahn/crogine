/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#pragma once

#include <crogine/core/String.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>

//NOTE Banner A should be rotated 180 degrees
static inline constexpr cro::FloatRect PlaneBannerA = { 12.f, 86.f, 484.f, 66.f };
static inline constexpr cro::FloatRect PlaneBannerB = { 12.f, 6.f, 484.f, 66.f };
static inline constexpr std::uint32_t BannerTextSize = 16;

inline void updateBannerTexture(const cro::Font& f, cro::TextureID src, cro::RenderTexture& dst, const cro::String& msg)
{
    cro::SimpleQuad q;
    q.setTexture(src, dst.getSize());

    cro::SimpleText t(f);
    t.setCharacterSize(BannerTextSize);
    t.setFillColour(TextNormalColour);
    t.setShadowColour(LeaderboardTextDark);
    t.setShadowOffset({ 2.f, -2.f });
    t.setString(msg);
    t.setAlignment(cro::SimpleText::Alignment::Centre);
    t.setVerticalSpacing(2.f);

    t.setPosition(glm::vec2(PlaneBannerB.left + (PlaneBannerB.width / 2.f), PlaneBannerB.bottom + (PlaneBannerB.height / 2.f) + (t.getVerticalSpacing() / 2.f)));

    dst.clear(cro::Colour::Transparent);
    q.draw();
    t.draw();
    t.rotate(180.f);
    t.move(glm::vec2(0.f, PlaneBannerA.bottom - PlaneBannerB.bottom));
    t.move(glm::vec2(0.f, -t.getVerticalSpacing()));
    t.draw();
    dst.display();
}