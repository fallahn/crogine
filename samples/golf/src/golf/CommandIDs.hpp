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
        BullsEye        = 0x40000
    };

    struct UI final
    {
        enum
        {
            ClubName             = 0x1,
            PlayerName           = 0x2,
            UIElement            = 0x4, //has its position updated on UI layout
            Root                 = 0x8,
            ThinkBubble          = 0x10,
            WindSock             = 0x20,
            WindSpeed            = 0x40,
            WindString           = 0x80,
            HoleNumber           = 0x100,
            PinDistance          = 0x200,
            Scoreboard           = 0x400,
            ScoreScroll          = 0x800,
            ScoreboardController = 0x1000,
            MiniMap              = 0x2000,
            MiniBall             = 0x4000,
            MessageBoard         = 0x8000,
            MiniGreen            = 0x10000,
            MiniFlag             = 0x20000,
            DrivingBoard         = 0x40000,
            StrengthMeter        = 0x80000,
            PlayerIcon           = 0x100000,
            WaitMessage          = 0x200000,
            FastForward          = 0x400000,
            WindHidden           = 0x800000, //I can't think of a better name - basically when the wind indicator is hidden during putting
            WindEffect           = 0x1000000,
            PuttPower            = 0x2000000,
            TerrainType          = 0x4000000,
            AFKWarn              = 0x8000000
        };
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
            ChatHint     = 0x400000
        };
    };
};