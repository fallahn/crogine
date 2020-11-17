/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include <crogine/graphics/MaterialData.hpp>

#include <cstdint>
#include <string>
#include <array>

struct MaterialDefinition final
{
    std::string name = "Untitled";

    enum Type
    {
        Unlit, VertexLit, PBR,

        Count
    }type = Unlit;

    std::uint32_t diffuse = 0;
    std::uint32_t mask = 0;
    std::uint32_t normal = 0;
    std::uint32_t lightmap = 0;

    cro::Colour colour;
    cro::Colour maskColour;

    float alphaClip = 0.f;

    bool vertexColoured = false;
    bool skinned = false; //TODO this could be inferred from the model to which it's applied?
    bool recieveShadows = false;

    cro::Material::BlendMode blendMode = cro::Material::BlendMode::None;
};
