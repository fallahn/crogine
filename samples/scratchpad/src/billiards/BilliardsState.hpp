/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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
#include "../collision/DebugDraw.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/CubemapTexture.hpp>

class BilliardsState final : public cro::State
{
public:
    BilliardsState(cro::StateStack&, cro::State::Context);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return States::ScratchPad::Billiards; }

private:

    cro::Clock m_interpClock;

    cro::Scene m_scene;
    cro::Scene m_skyboxScene;
    cro::ResourceCollection m_resources;

    cro::CubemapTexture m_cubemap;

    BulletDebug m_debugDrawer;
    cro::ModelDefinition m_ballDef;

    struct Camera final
    {
        enum
        {
            Overhead, Angle,
            Front, Side, Top,
            Count
        };
    };
    std::array<cro::Entity, Camera::Count> m_cameras = {};
    float m_fov;

    cro::Entity m_gyre;
    cro::Entity m_gimbal;

    void addSystems();
    void buildScene();

    void addBall();
};