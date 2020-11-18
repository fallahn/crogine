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
#include <crogine/graphics/RenderTexture.hpp>

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

    enum TextureID
    {
        Diffuse,
        Mask,
        Normal,
        Lightmap,

        Size
    };
    std::array<std::uint32_t, TextureID::Size> textureIDs = {};

    cro::Colour colour = glm::vec4(1.f);
    cro::Colour maskColour;
    cro::Colour rimlightColour = glm::vec4(1.f);

    float alphaClip = 0.f;
    float rimlightFalloff = 0.f;

    bool vertexColoured = false;
    bool useRimlighing = false;
    bool receiveShadows = false;

    cro::Material::BlendMode blendMode = cro::Material::BlendMode::None;


    //used for rendering preview
    cro::RenderTexture previewTexture;
    std::int32_t shaderFlags = 0; //used to detect when the shader needs to change
    Type activeType = Unlit;
    std::int32_t shaderID = -1; //shader ID in the resource manager, not OpenGL ID
    cro::Material::Data materialData;

    //hmmm is there not a better way to default init an array?
    MaterialDefinition()
    {
        std::fill(textureIDs.begin(), textureIDs.end(), 0);
        previewTexture.create(256, 256);
        previewTexture.setSmooth(true);
    }
};
