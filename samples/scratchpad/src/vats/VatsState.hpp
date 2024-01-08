/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/ArrayTexture.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <array>

class VatsState final : public cro::State, public cro::GuiClient
{
public:
    VatsState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::VATs; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Entity m_model;
    cro::Entity m_reference;

    cro::ResourceCollection m_resources;
    cro::Texture m_positionTexture;
    cro::Texture m_normalTexture;

    cro::ArrayTexture<float, 4u> m_arrayTexture;

    struct ShaderID final
    {
        enum
        {
            NoVats,
            Vats,
            VatsArray,
            VatsInstanced,

            Count
        };
    };

    struct MaterialID final
    {
        enum
        {
            NoVats,
            Vats,
            VatsArray,
            VatsInstanced,

            Count
        };
    };

    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    void addSystems();
    void loadAssets();
    void createScene();

    void loadModel(const std::string&);

    void createNormalTexture();
};