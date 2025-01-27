/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

struct CommandID final
{
    enum
    {
        Ball            = 0x1,
        StrokeIndicator = 0x2,
        Flag            = 0x4,
        Hole            = 0x8,
        Tee             = 0x10,
        Cart            = 0x20,
        SlopeIndicator  = 0x40,
        Crowd           = 0x80,
        SpectatorCam    = 0x100,
        PlayerAvatar    = 0x200,
        ParticleEmitter = 0x400,
        AudioEmitter    = 0x800,
        Beacon          = 0x1000,
        StrokeArc       = 0x2000,
        Spectator       = 0x4000,
        BeaconColour    = 0x8000,
        DroneCam        = 0x10000,
        HoleRing        = 0x20000,
        BullsEye        = 0x40000,
        Ghost           = 0x80000
    };

    struct UI final
    {
        //enum
        //{
            static constexpr std::uint64_t ClubName             = 0x1;
            static constexpr std::uint64_t PlayerName           = 0x2;
            static constexpr std::uint64_t UIElement            = 0x4; //has its position updated on UI layout
            static constexpr std::uint64_t Root                 = 0x8;
            static constexpr std::uint64_t ThinkBubble          = 0x10;
            static constexpr std::uint64_t WindSock             = 0x20;
            static constexpr std::uint64_t WindSpeed            = 0x40;
            static constexpr std::uint64_t WindString           = 0x80;
            static constexpr std::uint64_t HoleNumber           = 0x100;
            static constexpr std::uint64_t PinDistance          = 0x200;
            static constexpr std::uint64_t Scoreboard           = 0x400;
            static constexpr std::uint64_t ScoreScroll          = 0x800;
            static constexpr std::uint64_t ScoreboardController = 0x1000;
            static constexpr std::uint64_t MiniMap              = 0x2000;
            static constexpr std::uint64_t MiniBall             = 0x4000;
            static constexpr std::uint64_t MessageBoard         = 0x8000;
            static constexpr std::uint64_t MiniGreen            = 0x10000;
            static constexpr std::uint64_t MiniFlag             = 0x20000;
            static constexpr std::uint64_t DrivingBoard         = 0x40000;
            static constexpr std::uint64_t StrengthMeter        = 0x80000;
            static constexpr std::uint64_t PlayerIcon           = 0x100000;
            static constexpr std::uint64_t WaitMessage          = 0x200000;
            static constexpr std::uint64_t FastForward          = 0x400000;
            static constexpr std::uint64_t WindHidden           = 0x800000; //I can't think of a better name - basically when the wind indicator is hidden during putting
            static constexpr std::uint64_t WindEffect           = 0x1000000;
            static constexpr std::uint64_t PuttPower            = 0x2000000;
            static constexpr std::uint64_t TerrainType          = 0x4000000;
            static constexpr std::uint64_t AFKWarn              = 0x8000000;
            static constexpr std::uint64_t PuttingLabel         = 0x10000000; //29
            static constexpr std::uint64_t ScoreTitle           = 0x20000000; //text at the bottom of the score board - used in skins games
            static constexpr std::uint64_t PinHeight            = 0x40000000;
            static constexpr std::uint64_t BarEnt               = 0x80000000;
            static constexpr std::uint64_t PowerBarInner        = 0x100000000;
            static constexpr std::uint64_t BarEntLarge          = 0x200000000;
            static constexpr std::uint64_t PowerBarInnerLarge   = 0x400000000;

            static constexpr std::uint64_t GarbageCollect = 0x1; //this is the same as the first entry! we're just renaming it for use in the Scrub game
        //}
    };

    struct Menu final
    {
        enum
        {
            RootNode     = 0x1,
            ReadyButton  = 0x2,
            LobbyList    = 0x4,
            ServerInfo   = 0x8,
            PlayerConfig = 0x10,
            InfoString   = PlayerConfig,

            PlayerName   = 0x20,
            PlayerAvatar = 0x40,
            UIElement    = 0x80,
            UIBanner     = 0x100,
            CourseTitle  = 0x200,
            CourseDesc   = 0x400,
            CourseHoles  = 0x800,
            CourseSelect = 0x1000,
            TitleText    = 0x2000,
            ScoreType    = 0x4000,
            ScoreDesc    = 0x8000,
            GimmeDesc    = 0x10000,
            LobbyText    = 0x20000,
            CourseRules  = 0x40000,
            CourseType   = 0x80000,
            MetricClub   = 0x100000, //used in the stats viewer for club labels
            ImperialClub = 0x200000,
            ChatHint     = 0x400000,
            CourseHint   = 0x800000
        };
    };
};