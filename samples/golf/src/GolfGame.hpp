/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "DefaultAchievements.hpp"
#include "golf/SharedStateData.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/StateStack.hpp>
#include <crogine/core/Cursor.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <memory>

class GolfGame final : public cro::App, public cro::GuiClient
{
public:
    GolfGame();

    static cro::RenderTarget* getActiveTarget() { return m_renderTarget; }

private:
    
    SharedStateData m_sharedData;
    cro::StateStack m_stateStack;
    cro::Cursor m_cursor;

    std::vector<std::string> m_hostAddresses;

    std::unique_ptr<cro::RenderTexture> m_postBuffer;
    std::unique_ptr<cro::SimpleQuad> m_postQuad;
    std::unique_ptr<cro::Shader> m_postShader;
    std::int32_t m_postProcessIndex;
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

    //this contains GL resources so we need to control its lifetime with initialise / finialise
    std::unique_ptr<DefaultAchievements> m_achievements; 

    void handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    void simulate(float) override;
    void render() override;
    bool initialise() override;
    void finalise() override;

    void loadPreferences();
    void savePreferences();
    void recreatePostProcess();
};