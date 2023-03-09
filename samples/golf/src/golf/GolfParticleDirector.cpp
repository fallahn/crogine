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

#include "GolfParticleDirector.hpp"
#include "MessageIDs.hpp"
#include "Terrain.hpp"
#include "GameConsts.hpp"
#include "BallSystem.hpp"
#include "Clubs.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/SpriteSystem3D.hpp>

#include <crogine/graphics/TextureResource.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Constants.hpp>

GolfParticleDirector::GolfParticleDirector(cro::TextureResource& tr)
{
    m_emitterSettings[ParticleID::Water].loadFromFile("assets/golf/particles/water.cps", tr);
    m_emitterSettings[ParticleID::Grass].loadFromFile("assets/golf/particles/dirt.cps", tr);
    m_emitterSettings[ParticleID::GrassDark].loadFromFile("assets/golf/particles/grass_dark.cps", tr);
    m_emitterSettings[ParticleID::Sand].loadFromFile("assets/golf/particles/sand.cps", tr);
    m_emitterSettings[ParticleID::Sparkle].loadFromFile("assets/golf/particles/new_ball.cps", tr);
    m_emitterSettings[ParticleID::HIO].loadFromFile("assets/golf/particles/hio.cps", tr);
    m_emitterSettings[ParticleID::Bird].loadFromFile("assets/golf/particles/bird01.cps", tr);
    m_emitterSettings[ParticleID::Drone].loadFromFile("assets/golf/particles/drone.cps", tr);
    m_emitterSettings[ParticleID::Explode].loadFromFile("assets/golf/particles/explode.cps", tr);
    m_emitterSettings[ParticleID::Blades].loadFromFile("assets/golf/particles/blades.cps", tr);
    m_emitterSettings[ParticleID::Puff].loadFromFile("assets/golf/particles/puff.cps", tr);

    //hmm how to set smoothing on the texture?
    cro::SpriteSheet spriteSheet;
    if (spriteSheet.loadFromFile("assets/golf/sprites/rings.spt", tr))
    {
        m_ringSprite = spriteSheet.getSprite("rings");
        const_cast<cro::Texture*>(m_ringSprite.getTexture())->setSmooth(false); //yucky.
    }
}

//public
void GolfParticleDirector::handleMessage(const cro::Message& msg)
{
    const auto getEnt = [&](std::int32_t id, glm::vec3 position)
    {
        auto entity = getNextEntity();
        entity.getComponent<cro::Transform>().setPosition(position);
        entity.getComponent<cro::ParticleEmitter>().settings = m_emitterSettings[id];
        entity.getComponent<cro::ParticleEmitter>().start();
        return entity;
    };

    switch (msg.id)
    {
    default: break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::ClubSwing)
        {
            switch (data.terrain)
            {
            default: break;
            case TerrainID::Rough:
                getEnt(ParticleID::GrassDark, data.position);
                break;
            case TerrainID::Fairway:
                getEnt(ParticleID::Grass, data.position);
                break;
            case TerrainID::Bunker:
                getEnt(ParticleID::Sand, data.position);
                break;
            case TerrainID::Water:
                getEnt(ParticleID::Water, data.position);
                break;
            }
        }
        else if (data.type == GolfEvent::SetNewPlayer)
        {
            if (data.travelDistance == -1)
            {
                auto pos = data.position;
                pos.y += (Ball::Radius * 2.f);
                getEnt(ParticleID::Puff, pos);
            }
        }
        else if (data.type == GolfEvent::HoleInOne)
        {
            getEnt(ParticleID::HIO, data.position);
        }
        else if (data.type == GolfEvent::DroneHit)
        {
            getEnt(ParticleID::Drone, data.position);
            getEnt(ParticleID::Explode, data.position);
            getEnt(ParticleID::Blades, data.position);
        }
        else if (data.type == GolfEvent::BirdHit)
        {
            getEnt(ParticleID::Bird, data.position).getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, data.travelDistance + (cro::Util::Const::PI / 2.f));
        }
        else if (data.type == GolfEvent::BallLanded)
        {
            if (data.club == ClubID::Putter &&
                (data.terrain == TerrainID::Water || data.terrain == TerrainID::Scrub))
            {
                //assume we putt off the green on a putting course
                auto pos = data.position;
                pos.y += (Ball::Radius * 2.f);
                getEnt(ParticleID::Puff, pos);
            }
        }
    }
        break;
    case MessageID::CollisionMessage:
    {
        const auto& data = msg.getData<CollisionEvent>();

        if (data.type == CollisionEvent::Begin)
        {
            switch (data.terrain)
            {
            default: break;
            case TerrainID::Rough:
                getEnt(ParticleID::GrassDark, data.position);
                break;
            case TerrainID::Bunker:
                getEnt(ParticleID::Sand, data.position);
                break;
            case TerrainID::Water:
                getEnt(ParticleID::Water, data.position);
                spawnRings(data.position);
                break;
            }
        }
    }
    break;
    }
}

//private
void GolfParticleDirector::spawnRings(glm::vec3 position)
{
    position.y = WaterLevel + 0.001f;
    auto bounds = m_ringSprite.getTextureBounds();
    float scale = getScene().getSystem<cro::SpriteSystem3D>()->getPixelsPerUnit();

    auto entity = getScene().createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.getComponent<cro::Transform>().setOrigin({ (bounds.width / 2.f) / scale, (bounds.height / 2.f) / scale, 0.f });
    entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, 90.f * cro::Util::Const::degToRad);
    entity.addComponent<cro::Sprite>() = m_ringSprite;
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<cro::Model>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (!e.getComponent<cro::SpriteAnimation>().playing)
        {
            getScene().destroyEntity(e);
        }
    };
}