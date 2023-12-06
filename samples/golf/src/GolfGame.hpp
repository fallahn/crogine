/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#ifndef USE_GNS
#include "DefaultAchievements.hpp"
#else
#include <Achievements.hpp>
#endif
#include "golf/SharedStateData.hpp"
#include "golf/SharedProfileData.hpp"
#include "golf/ProgressIcon.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/ConsoleClient.hpp>
#include <crogine/core/StateStack.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <memory>

class GolfGame final : public cro::App, public cro::GuiClient, public cro::ConsoleClient
{
public:
    GolfGame();

    static cro::RenderTarget* getActiveTarget() { return m_renderTarget; }

    void loadPlugin(const std::string& path) { cro::App::loadPlugin(path, m_stateStack); }
    void unloadPlugin() { cro::App::unloadPlugin(m_stateStack); }

private:
    
    SharedStateData m_sharedData;
    SharedProfileData m_profileData;
    cro::StateStack m_stateStack;

    std::vector<std::string> m_hostAddresses;

    std::unique_ptr<ProgressIcon> m_progressIcon;

    std::unique_ptr<cro::RenderTexture> m_postBuffer;
    std::unique_ptr<cro::SimpleQuad> m_postQuad;
    std::unique_ptr<cro::Shader> m_postShader;
    std::int32_t m_activeIndex;

    struct UniformID final
    {
        enum
        {
            Time,
            Scale,

            Count
        };
    };
    std::array<std::int32_t, UniformID::Count> m_uniformIDs = {};

    static cro::RenderTarget* m_renderTarget;
    static cro::App* m_instance;

#ifndef USE_GNS
    //this contains GL resources so we need to control its lifetime with initialise / finialise
    std::unique_ptr<DefaultAchievements> m_achievements;
#else
    std::unique_ptr<AchievementImpl> m_achievements; 
#endif

    void handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    void simulate(float) override;
    void render() override;
    bool initialise() override;
    void finalise() override;

    void initFonts();
    void convertPreferences() const;
    void loadPreferences();
    void savePreferences();
    void loadAvatars();
    void recreatePostProcess();
    void applyPostProcess();
    bool setShader(const char*);
};