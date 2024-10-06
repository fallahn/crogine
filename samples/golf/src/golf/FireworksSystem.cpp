/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "FireworksSystem.hpp"
#include "MessageIDs.hpp"
#include "GameConsts.hpp"
#include "../Colordome-32.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

using namespace cl;
namespace
{
    static constexpr std::size_t MinSpawnCount = 6;
    static constexpr std::size_t MaxSpawnCount = 15;

    static constexpr std::int32_t MinSpawnTime = 1;
    static constexpr std::int32_t MaxSpawnTime = 3;

    static constexpr std::int32_t MinSize = 1; //actually a tenth of this - see randomiser
    static constexpr std::int32_t MaxSize = 9;

    static constexpr std::array<cro::Colour, 9u> Colours =
    {
        CD32::Colours[CD32::BeigeLight],
        CD32::Colours[CD32::GreyLight],
        CD32::Colours[CD32::BlueLight],
        CD32::Colours[CD32::GreenLight],
        CD32::Colours[CD32::Yellow],
        CD32::Colours[CD32::Orange],
        CD32::Colours[CD32::Red],

        cro::Colour(1.f, 0.f, 1.f),
        cro::Colour(0.f, 1.f, 1.f),
    };
}

FireworksSystem::FireworksSystem(cro::MessageBus& mb, const cro::Mesh::Data& mesh, const cro::Material::Data& mat)
    : cro::System   (mb, typeid(FireworksSystem)),
    m_meshData      (mesh),
    m_materialData  (mat),
    m_spawnIndex    (0),
    m_spawnCount    (MinSpawnCount),
    m_spawnTime     (0.f)
{
    requireComponent<cro::Transform>();
    requireComponent<Firework>();
}

//public
void FireworksSystem::process(float dt)
{
    for (auto entity : getEntities())
    {
        auto& fw = entity.getComponent<Firework>();
        auto& tx = entity.getComponent<cro::Transform>();

        fw.prevProgress = fw.progress;
        fw.progress = std::min(1.f, fw.progress + dt);

        const float materialProgress = std::max(0.f, fw.progress);
        if (fw.progressProperty)
        {
            fw.progressProperty->numberValue = materialProgress;
            fw.progressProperty->type = cro::Material::Property::Number;
        }

        const float scale = cro::Util::Easing::easeOutExpo(materialProgress) * fw.maxRadius;
        //const float scale = cro::Util::Easing::easeOutCirc(std::max(0.f, fw.progress)) * fw.maxRadius;
        tx.setScale(glm::vec3(scale));

        if (fw.progress == 1.f)
        {
            getScene()->destroyEntity(entity);
        }

        //used by audio director
        if (fw.progress >= 0
            && fw.prevProgress < 0)
        {
            auto* msg = postMessage<EnviroEvent>(MessageID::EnviroMessage);
            msg->position = (tx.getPosition() * 60.f) + glm::vec3(MapSizeFloat.x / 2.f, 0.f, -MapSizeFloat.y / 2.f);
            msg->size = 0.7f + (0.3f - (fw.maxRadius / 3.f)); //controls the pitch
        }
    }

    //check time to spawn
    if (m_spawnClock.elapsed().asSeconds() > m_spawnTime)
    {
        CRO_ASSERT(!m_positions.empty(), "No positions have been added");

        for (auto i = 0u; i < m_spawnCount; ++i)
        {
            const float size = static_cast<float>(cro::Util::Random::value(MinSize, MaxSize)) / 10.f;
            auto entity = getScene()->createEntity();
            entity.addComponent<cro::Transform>().setPosition(m_positions[m_spawnIndex]);
            entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            //nice idea but we need to fix gravity in the shader if we do this
            /*entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, cro::Util::Random::value(-cro::Util::Const::PI, cro::Util::Const::PI));
            entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, cro::Util::Random::value(-cro::Util::Const::PI, cro::Util::Const::PI));
            entity.getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, cro::Util::Random::value(-cro::Util::Const::PI, cro::Util::Const::PI));*/
            entity.addComponent<cro::Model>(m_meshData, m_materialData);
            entity.addComponent<Firework>().maxRadius = size;
            entity.getComponent<Firework>().progress = -static_cast<float>(i) / 7.f;
            entity.getComponent<Firework>().progress -= cro::Util::Random::value(0.05f, 0.18f);

            //stash the material property so we can update it without having to do repeated lookups
            auto& materialProperties = entity.getComponent<cro::Model>().getMaterialData(cro::Mesh::IndexData::Final, 0).properties;
            if (auto progressProperty = materialProperties.find("u_progress"); progressProperty != materialProperties.end())
            {
                entity.getComponent<Firework>().progressProperty = &progressProperty->second.second;
            }

            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", Colours[m_spawnIndex % Colours.size()]);
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_size", size);

            m_spawnIndex = (m_spawnIndex + 1) % m_positions.size();
        }
        m_spawnCount = cro::Util::Random::value(MinSpawnCount, MaxSpawnCount);

        m_spawnClock.restart();
        m_spawnTime = static_cast<float>(cro::Util::Random::value(MinSpawnTime, MaxSpawnTime));
    }
}