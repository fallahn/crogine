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

static constexpr inline std::uint32_t Smiley         = 0x1F600;
static constexpr inline std::uint32_t Grinning       = 0x1F601;
static constexpr inline std::uint32_t Laughing       = 0x1F602;
static constexpr inline std::uint32_t StarEyed       = 0x1F929;
static constexpr inline std::uint32_t TonguePoke     = 0x1F61B;
static constexpr inline std::uint32_t Thinking       = 0x1F914;
static constexpr inline std::uint32_t Chuckle        = 0x1F92D;
static constexpr inline std::uint32_t PartyFace      = 0x1F973;
static constexpr inline std::uint32_t SunGlasses     = 0x1F60E;
static constexpr inline std::uint32_t EyeRoll        = 0x1F644;
static constexpr inline std::uint32_t Grimace        = 0x1F62C;
static constexpr inline std::uint32_t Surprised      = 0x1F632;
static constexpr inline std::uint32_t Embaressed     = 0x1F633;
static constexpr inline std::uint32_t Disappointed   = 0x1F629;
static constexpr inline std::uint32_t Angry          = 0x1F624;
static constexpr inline std::uint32_t GoldCup        = 0x1F3C6;
static constexpr inline std::uint32_t GoldMedal      = 0x1F947;
static constexpr inline std::uint32_t SilverMedal    = 0x1F948;
static constexpr inline std::uint32_t BronzeMedal    = 0x1F949;
static constexpr inline std::uint32_t GolfFlag       = 0x26F3;
static constexpr inline std::uint32_t RedHeart       = 0x2764;
static constexpr inline std::uint32_t Hole           = 0x1F573;
static constexpr inline std::uint32_t Explosive      = 0x1F4A5;
static constexpr inline std::uint32_t Windy          = 0x1F4A8;
static constexpr inline std::uint32_t Sleeping       = 0x1F4A4;
static constexpr inline std::uint32_t FriedEgg       = 0x1F373;
static constexpr inline std::uint32_t Birdie         = 0x1F426;
static constexpr inline std::uint32_t Eagle          = 0x1F985;
static constexpr inline std::uint32_t Snake          = 0x1F40D;
static constexpr inline std::uint32_t Crocodile      = 0x1F40A;

static constexpr inline std::uint32_t Mist           = 0x1F32B; //requires term
static constexpr inline std::uint32_t Rainbow        = 0x1F308;
static constexpr inline std::uint32_t Umbrella       = 0x2614;
static constexpr inline std::uint32_t RainCloud      = 0x1F327; //requires term
static constexpr inline std::uint32_t Snow           = 0x2744;  //requires term
static constexpr inline std::uint32_t Sun            = 0x2600;  //requires term
static constexpr inline std::uint32_t Moon           = 0x1F319;
static constexpr inline std::uint32_t Calendar       = 0x1F4C5;
static constexpr inline std::uint32_t Warning        = 0x26A0;
static constexpr inline std::uint32_t EmojiTerminate = 0xFE0F;