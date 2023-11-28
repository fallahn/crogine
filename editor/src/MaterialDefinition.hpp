/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "UIConsts.hpp"

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
    }type = VertexLit;

    enum TextureID
    {
        Diffuse,
        Mask,
        Normal,
        Lightmap,

        Size
    };
    std::array<std::uint32_t, TextureID::Size> textureIDs = {};

    cro::Colour colour = glm::vec4(1.f, 1.f, 1.f, 1.f);
    cro::Colour maskColour = glm::vec4(1.f, 1.f, 0.f, 1.f);
    cro::Colour rimlightColour = glm::vec4(1.f);

    float alphaClip = 0.f;
    float rimlightFalloff = 0.f;

    glm::vec4 subrect = glm::vec4(0.f, 0.f, 1.f, 1.f);

    bool useSubrect = false;
    bool vertexColoured = false;
    bool useRimlighing = false;
    bool receiveShadows = false;
    bool skinned = false;
    bool depthTest = true;
    bool doubleSided = false;

    bool smoothTexture = true;
    bool repeatTexture = true;
    bool useMipmaps = false;

    bool animated = false;
    float frameRate = 1.f;
    std::uint32_t rowCount = 1;
    std::uint32_t colCount = 1;

    cro::Material::BlendMode blendMode = cro::Material::BlendMode::None;
    std::vector<std::string> tags;

    //used for rendering preview
    cro::RenderTexture previewTexture;
    std::int32_t shaderFlags = 0; //used to detect when the shader needs to change
    Type activeType = Unlit;
    std::int32_t shaderID = -1; //shader ID in the resource manager, not OpenGL ID
    cro::Material::Data materialData;

    std::vector<std::uint32_t> submeshIDs; //if this material is applied to any meshes on the currently open model, the IDs are here.

    //hmmm is there not a better way to default init an array?
    MaterialDefinition()
    {
        std::fill(textureIDs.begin(), textureIDs.end(), 0);
        previewTexture.create(uiConst::PreviewTextureSize, uiConst::PreviewTextureSize);
        previewTexture.setSmooth(true);
    }
};
