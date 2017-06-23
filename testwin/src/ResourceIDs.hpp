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
        GameBackgroundFar,
        GameBackgroundMid,
        GameBackgroundNear,
        TerrainChunk,
        Rockfall, Rockfall2
    };
}

namespace MeshID
{
    enum
    {
        GameBackground = cro::Mesh::ID::Count,
        TerrainChunkA,
        TerrainChunkB,
        RockQuad
    };
}

namespace MenuModelID
{
    enum
    {
        LookoutBase = 0,
        ArcticPost,
        GasPlanet,
        Moon,
        Roids,
        Stars,
        Sun,
        Count
    };
}

namespace GameModelID
{
    enum
    {
        Player = 0,
        PlayerShield,
        CollectableBatt,
        CollectableBomb,
        CollectableBot,
        CollectableHeart,
        CollectableShield,
        Elite,
        TurretBase, TurretCannon,
        Choppa,
        Speedray,
        WeaverHead,
        WeaverBody,
        Count
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
        Background //< scrolling game background
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
        Turret = 0x20,
        Elite = 0x40,
        Choppa = 0x80,
        Speedray = 0x100,
        Charger = 0x200,
        Weaver = 0x400
    };
}

namespace CollisionID
{
    enum
    {
        Player = 1,
        NPC = 2,
        Collectable = 4,
        Environment = 8,
        Bounds = 16,
        PlayerLaser = 32
    };
}

#endif //TL_RESOURCE_IDS_HPP_