/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef TL_RESOURCE_IDS_HPP_
#define TL_RESOURCE_IDS_HPP_

#include <crogine/graphics/MeshResource.hpp>

namespace MaterialID
{
    enum
    {
        Moon,
        Stars,
        Planet,
        PlanetClouds,
        Roids,
        GameBackgroundFar,
        GameBackgroundMid,
        GameBackgroundNear,
        PlayerShip,
        TerrainChunk,
        Rockfall, Rockfall2, //hmm a bit hacky but we want a variant for each entity
        BattCollectable, BombCollectable,
        BotCollectable, HeartCollectable,
        ShieldCollectable,
        NPCElite, NPCChoppa,
        NPCTurretBase, NPCTurretCannon
    };
}

namespace MeshID
{
    enum
    {
        Roids = cro::Mesh::ID::Count,
        Moon,
        GameBackground,
        PlayerShip,
        TerrainChunkA,
        TerrainChunkB,
        RockQuad,
        Collectable,
        NPCElite, NPCChoppa,
        NPCTurretBase, NPCTurretCannon
    };
}

namespace FontID
{
    enum
    {
        MenuFont
    };
}

namespace ShaderID
{
    enum
    {
        Background
    };
}

namespace CommandID
{
    enum
    {
        MenuController = 0x1,
        RockParticles = 0x2,
        SnowParticles = 0x4,
        Player = 0x8,
        Collectable = 0x10,
        Turret = 0x20
    };
}

#endif //TL_RESOURCE_IDS_HPP_