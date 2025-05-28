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

//enumerates the selection indices for Options menu UIInput components
enum OptionsIndex
{
    Tab = 100,
    TabAV,
    TabController,
    TabSettings,
    TabAchievements,
    TabStats,

    AV = 200,
    AVMixerLeft,
    AVMixerRight,
    AVVolumeDown,
    AVVolumeUp,
    AVDeviceDown,
    AVDeviceUp,
    AVAAL,
    AVAAR,
    AVFOVL,
    AVFOVR,
    AVResolutionL,
    AVResolutionR,
    AVPixelScale,
    AVVertSnap,
    AVFullScreen,
    AVFullScreenMode,
    AVVSync,
    AVBeacon,
    AVBeaconL,
    AVBeaconR,
    AVLightMap,
    AVLensFlare,
    AVTextToSpeech,
    AVTrail,
    AVTrailL,
    AVTrailR,
    AVPuttAss,
    AVUnits,
    AVGridL,
    AVGridR,
    AVTreeL,
    AVTreeR,
    AVShadowL,
    AVShadowR,
    AVCrowdL,
    AVCrowdR,
    AVLargePower,
    AVContrastPower,
    AVDecPower,
    AVDecDist,
    AVFixedPutter,


    Controls = 300,
    CtrlThreshL,
    CtrlThreshR,
    CtrlLayout,
    CtrlLookL,
    CtrlLookR,
    CtrlA,
    CtrlB,
    CtrlX,
    CtrlY,
    CtrlDown,
    CtrlUp,
    CtrlRight,
    CtrlLeft,
    CtrlRB,
    CtrlLB,
    CtrlInvX,
    CtrlInvY,
    CtrlMouseAction,
    CtrlVib,
    CtrlAltPower,
    CtrlSwg,
    CtrlReset,
    CtrlTab,
    CtrlButtonCount,

    Settings = 400,
    SettWebsockEnable,
    SettWebPortDown,
    SettWebPortUp,
    SettCSVLog,
    SettInGameChat,
    SettLogChat,
    SettRemoteContent,
    SettShowRival,
    SettPuttFollow,
    SettPuttZoom,
    SettFlagDown,
    SettFlagUp,
    SettFlagLabelDown,
    SettFlagLabelUp,
    SettPost,
    SettPostL,
    SettPostR,

    
    Scroll = 1000,
    ScrollUp,
    ScrollDown,
    ResetStats,
    ResetCareer,


    Window = 2000,
    WindowCredits,
    WindowAdvanced,
    WindowHowTo,
    WindowApply,
    WindowClose
};