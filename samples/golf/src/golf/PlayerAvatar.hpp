/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "PlayerColours.hpp"
#include "PlayerData.hpp"

#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Colour.hpp>

#include <string>

class PlayerAvatar final : public ProfileTexture
{
public:
    explicit PlayerAvatar(const std::string&);

    cro::Entity previewModel;

    struct HairInfo final
    {
        std::uint32_t uid = 0;
        cro::Entity model;
    };
    std::vector<HairInfo> hairModels;
    cro::Attachment* hairAttachment = nullptr;
    cro::Attachment* hatAttachment = nullptr;

    std::vector<cro::Entity> previewSounds;

private:

};
