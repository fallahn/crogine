/*-----------------------------------------------------------------------

Matt Marchant 2026
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

#ifdef _WIN32
#include "NVSettings.hpp"

#include <nvapi.h>
#include <NvApiDriverSettings.h>

#include <iostream>
#include <string>

namespace
{
    //const bool threadedOptimisation = false;

    const std::wstring profileName = L"Super Video Golf";
#ifdef USE_GNS
    const std::wstring appName = L"golf.exe";
#else
    const std::wstring appName = L"golf-nosteam.exe";
#endif
    const std::wstring appFriendlyName = L"Super Video Golf";
}

static inline void setStr(NvAPI_UnicodeString& out, const std::wstring& in)
{
    memcpy_s(out, sizeof(out), in.data(), in.size() * sizeof(wchar_t));
}

static inline void CheckError(NvAPI_Status status)
{
    if (status == NVAPI_OK)
    {
        return;
    }

    NvAPI_ShortString str = { 0 };
    NvAPI_GetErrorMessage(status, str);

    if (str)
    {
        //can't use normal logger here as it's not yet initialised
        std::cerr << str << std::endl;
        OutputDebugStringA(str);
    }
}

//https://stackoverflow.com/questions/36959508/nvidia-graphics-driver-causing-noticeable-frame-stuttering
void applyNVSettings()
{
    NvAPI_Status status = {};
    NvDRSSessionHandle hSession = 0;

    status = NvAPI_Initialize();
    CheckError(status);

    status = NvAPI_DRS_CreateSession(&hSession);
    CheckError(status);

    status = NvAPI_DRS_LoadSettings(hSession);
    CheckError(status);


    

    //fill Profile Info
    NVDRS_PROFILE profileInfo = {};
    profileInfo.version = NVDRS_PROFILE_VER;
    profileInfo.isPredefined = 0;
    setStr(profileInfo.profileName, profileName);

    //create Profile
    NvDRSProfileHandle hProfile = 0;
    status = NvAPI_DRS_CreateProfile(hSession, &profileInfo, &hProfile);
    CheckError(status);


    //fill Application Info
    NVDRS_APPLICATION app = {};
    app.version = NVDRS_APPLICATION_VER_V1;
    app.isPredefined = 0;
    setStr(app.appName, appName);
    setStr(app.userFriendlyName, appFriendlyName);
    //setStr(app.launcher, L"");
    //setStr(app.fileInFolder, L"");

    //create Application
    status = NvAPI_DRS_CreateApplication(hSession, hProfile, &app);
    CheckError(status);


    //fill Setting Info
    NVDRS_SETTING setting = {};
    setting.version = NVDRS_SETTING_VER;
    setting.settingId = OGL_THREAD_CONTROL_ID;
    setting.settingType = NVDRS_DWORD_TYPE;
    setting.settingLocation = NVDRS_CURRENT_PROFILE_LOCATION;
    setting.isCurrentPredefined = 0;
    setting.isPredefinedValid = 0;
    setting.u32CurrentValue = /*threadedOptimisation*/false ? OGL_THREAD_CONTROL_ENABLE : OGL_THREAD_CONTROL_DISABLE;
    setting.u32PredefinedValue = /*threadedOptimisation*/false ? OGL_THREAD_CONTROL_ENABLE : OGL_THREAD_CONTROL_DISABLE;

    //set Setting
    status = NvAPI_DRS_SetSetting(hSession, hProfile, &setting);
    CheckError(status);


    //apply (or save) our changes to the system
    status = NvAPI_DRS_SaveSettings(hSession);
    CheckError(status);

    NvAPI_DRS_DestroySession(hSession);
}
#endif