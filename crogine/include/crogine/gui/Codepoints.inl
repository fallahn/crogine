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

#pragma once

#include <cstdint>

/*
Inline file containing codepoints for various icons and fonts
*/

//-----------prompt font icons---------------//

//xbox
static constexpr inline std::uint32_t ButtonLT    = 0x2196;
static constexpr inline std::uint32_t ButtonRT    = 0x2197;
static constexpr inline std::uint32_t ButtonLB    = 0x2198;
static constexpr inline std::uint32_t ButtonRB    = 0x2199;
static constexpr inline std::uint32_t ButtonX     = 0x21D0;
static constexpr inline std::uint32_t ButtonY     = 0x21D1;
static constexpr inline std::uint32_t ButtonB     = 0x21D2;
static constexpr inline std::uint32_t ButtonA     = 0x21D3;
static constexpr inline std::uint32_t ButtonStart = 0x21FB;


//ps
static constexpr inline std::uint32_t ButtonL1       = 0x21B0;
static constexpr inline std::uint32_t ButtonR1       = 0x21B1;
static constexpr inline std::uint32_t ButtonL2       = 0x21B2;
static constexpr inline std::uint32_t ButtonR2       = 0x21B3;
static constexpr inline std::uint32_t ButtonSquare   = 0x21E0;
static constexpr inline std::uint32_t ButtonTriangle = 0x21E1;
static constexpr inline std::uint32_t ButtonCircle   = 0x21E2;
static constexpr inline std::uint32_t ButtonCross    = 0x21E3;
static constexpr inline std::uint32_t ButtonOption   = 0x21E8;


static constexpr inline std::uint32_t LeftStick  = 0x21EF;
static constexpr inline std::uint32_t RightStick = 0x21C6;



//keyboard
static constexpr std::uint32_t IconLeft      = 0x23F4;
static constexpr std::uint32_t IconRight     = 0x23F5;
static constexpr std::uint32_t IconUp        = 0x23F6;
static constexpr std::uint32_t IconDown      = 0x23F7;
static constexpr std::uint32_t IconShift     = 0x2429;
static constexpr std::uint32_t IconTab       = 0x242B;
static constexpr std::uint32_t IconCaps      = 0x242C;
static constexpr std::uint32_t IconBackspace = 0x242D;
static constexpr std::uint32_t IconReturn    = 0x242E;
static constexpr std::uint32_t IconSpace     = 0x243A;



//------------------emojis------------------//

static constexpr inline std::uint32_t EmSmiley         = 0x1F600;
static constexpr inline std::uint32_t EmGrinning       = 0x1F601;
static constexpr inline std::uint32_t EmLaughing       = 0x1F602;
static constexpr inline std::uint32_t EmStarEyed       = 0x1F929;
static constexpr inline std::uint32_t EmTonguePoke     = 0x1F61B;
static constexpr inline std::uint32_t EmThinking       = 0x1F914;
static constexpr inline std::uint32_t EmChuckle        = 0x1F92D;
static constexpr inline std::uint32_t EmPartyFace      = 0x1F973;
static constexpr inline std::uint32_t EmSunGlasses     = 0x1F60E;
static constexpr inline std::uint32_t EmEyeRoll        = 0x1F644;
static constexpr inline std::uint32_t EmGrimace        = 0x1F62C;
static constexpr inline std::uint32_t EmSurprised      = 0x1F632;
static constexpr inline std::uint32_t EmEmbaressed     = 0x1F633;
static constexpr inline std::uint32_t EmDisappointed   = 0x1F629;
static constexpr inline std::uint32_t EmAngry          = 0x1F624;
static constexpr inline std::uint32_t EmGoldCup        = 0x1F3C6;
static constexpr inline std::uint32_t EmGoldMedal      = 0x1F947;
static constexpr inline std::uint32_t EmSilverMedal    = 0x1F948;
static constexpr inline std::uint32_t EmBronzeMedal    = 0x1F949;
static constexpr inline std::uint32_t EmGolfFlag       = 0x26F3;
static constexpr inline std::uint32_t EmRedHeart       = 0x2764;
static constexpr inline std::uint32_t EmHole           = 0x1F573;
static constexpr inline std::uint32_t EmExplosive      = 0x1F4A5;
static constexpr inline std::uint32_t EmWindy          = 0x1F4A8;
static constexpr inline std::uint32_t EmSleeping       = 0x1F4A4;
static constexpr inline std::uint32_t EmFriedEgg       = 0x1F373;
static constexpr inline std::uint32_t EmBirdie         = 0x1F426;
static constexpr inline std::uint32_t EmEagle          = 0x1F985;
static constexpr inline std::uint32_t EmSnake          = 0x1F40D;
static constexpr inline std::uint32_t EmCrocodile      = 0x1F40A;
static constexpr inline std::uint32_t EmClapping       = 0x1F44F;
static constexpr inline std::uint32_t EmHysterics      = 0x1F923;

static constexpr inline std::uint32_t EmMist           = 0x1F32B; //requires term
static constexpr inline std::uint32_t EmRainbow        = 0x1F308;
static constexpr inline std::uint32_t EmUmbrella       = 0x2614;
static constexpr inline std::uint32_t EmRainCloud      = 0x1F327; //requires term
static constexpr inline std::uint32_t EmSnow           = 0x2744;  //requires term
static constexpr inline std::uint32_t EmSun            = 0x2600;  //requires term
static constexpr inline std::uint32_t EmMoon           = 0x1F319;
static constexpr inline std::uint32_t EmCalendar       = 0x1F4C5;
static constexpr inline std::uint32_t EmWarning        = 0x26A0;
static constexpr inline std::uint32_t EmojiTerminate   = 0xFE0F;