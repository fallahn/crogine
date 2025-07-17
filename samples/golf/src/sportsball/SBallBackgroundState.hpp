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

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

#include <crogine/gui/GuiClient.hpp>

#include <memory>

//#ifdef CRO_DEBUG_
//#ifdef _WIN32
//#include <sapi.h>
//#include <sphelper.h>
//#endif
//#endif

struct SharedStateData;
struct SharedMinigameData;
class SBallBackgroundState final : public cro::State
#ifdef CRO_DEBUG_
    , public cro::GuiClient
#endif
{
public:

    SBallBackgroundState(cro::StateStack&, cro::State::Context, const SharedStateData&, SharedMinigameData&);
    ~SBallBackgroundState();

    SBallBackgroundState(const SBallBackgroundState&) = delete;
    SBallBackgroundState(SBallBackgroundState&&) = delete;

    SBallBackgroundState& operator = (const SBallBackgroundState&) = delete;
    SBallBackgroundState& operator = (SBallBackgroundState&&) = delete;

    cro::StateID getStateID() const override { return StateID::SBallBackground; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    const SharedStateData& m_sharedData;
    SharedMinigameData& m_sharedGameData;

    cro::Scene m_scene;
    cro::ResourceCollection m_resources;
    cro::EnvironmentMap m_environmentMap;

    std::array<std::unique_ptr<cro::ModelDefinition>, 3u> m_backgroundModels;

    cro::RenderTexture m_renderTexture;
    cro::SimpleQuad m_renderQuad;

    void buildScene();

#ifdef CRO_DEBUG_
    /*struct ComInitRAII final
    {
        ISpVoice* voice = nullptr;
        IEnumSpObjectTokens* voiceList = nullptr;
        unsigned long voiceCount = 0;

        ISpObjectToken* selectedVoice = nullptr;

        ComInitRAII()
        {
            CoInitialize(NULL);
            CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&voice);

            SpEnumTokens(SPCAT_VOICES, NULL, NULL, &voiceList);
            voiceList->GetCount(&voiceCount);
        }
        ~ComInitRAII()
        {
            if (voiceList)
            {
                voiceList->Release();
            }

            if (voice)
            {
                voice->Release();
            }

            CoUninitialize();
        }
    }m_comInitRAII;
    
    void voiceWindow();*/
#endif
};