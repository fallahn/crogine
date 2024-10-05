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
#include "../Colordome-32.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

namespace
{
    static constexpr std::size_t MinSpawnCount = 6;
    static constexpr std::size_t MaxSpawnCount = 15;

    static constexpr std::int32_t MinSpawnTime = 1;
    static constexpr std::int32_t MaxSpawnTime = 5;

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
            entity.addComponent<cro::Model>(m_meshData, m_materialData);
            entity.addComponent<Firework>().maxRadius = size;
            entity.getComponent<Firework>().progress = -static_cast<float>(i) / 10.f;

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