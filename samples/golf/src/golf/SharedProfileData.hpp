/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "PlayerData.hpp"
#include "Inventory.hpp"

#include <crogine/graphics/ModelDefinition.hpp>

#include <vector>

struct SharedProfileData final
{
    //be careful here! these store a reference to the resources from the menu state
    //however it is used to load models in the profile state - so the profile state
    //must ALWAYS be loaded on top of the menu!!!
    std::vector<cro::ModelDefinition> ballDefs;
    std::vector<cro::ModelDefinition> avatarDefs;
    std::vector<cro::ModelDefinition> hairDefs;
    std::unique_ptr<cro::ModelDefinition> shadowDef;
    std::unique_ptr<cro::ModelDefinition> grassDef;

    struct
    {
        cro::Material::Data hair;
        cro::Material::Data hairReflection;
        cro::Material::Data hairGlass;
        cro::Material::Data ball;
        cro::Material::Data ballBumped;
        cro::Material::Data ballSkinned;
        cro::Material::Data ballReflection;
        cro::Material::Data avatar;

        void reset()
        {
            hair = hairReflection = hairGlass = ball = ballBumped = ballSkinned = ballReflection = avatar = {};
        }
    }profileMaterials;

    struct LocalProfile final
    {
        PlayerData playerData;
        inv::Loadout loadout;
    };
    std::vector<LocalProfile> playerProfiles;
    std::size_t activeProfileIndex = 0; //this indexes into the profile array when editing/deleting profiles
};