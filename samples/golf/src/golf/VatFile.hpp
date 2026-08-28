/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2026
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

#include <crogine/graphics/ArrayTexture.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class VatFile final
{
public:
    VatFile();

    bool loadFromFile(const std::string&);

    const std::filesystem::path& getModelPath() const { return m_modelPath; }
    const std::filesystem::path& getPositionPath() const;
    const std::filesystem::path& getNormalPath() const;
    const std::filesystem::path& getTangentPath() const;

    bool hasTangents() const;

    //returns false if there was no array texture to create
    //model texture is layer 0, followed by position, normal
    //and optionally tangent
    bool fillArrayTexture(cro::ArrayTexture<float, 4, cro::TexturePrecision::Low>&) const;

private:

    float m_frameRate;
    std::int32_t m_frameCount;
    std::int32_t m_frameLoop;

    std::filesystem::path m_modelPath;
    std::filesystem::path m_diffusePath;

    struct DataID final
    {
        enum
        {
            Position, Normal, Tangent,
            Count
        };
    };
    std::array<std::filesystem::path, DataID::Count> m_dataPaths = {};
    std::array<std::vector<float>, DataID::Count> m_binaryData = {};
    glm::uvec2 m_binaryDims;

    void loadBinary(const std::filesystem::path& path, std::vector<float>& dst, glm::uvec2 dims);

    void reset();

    friend struct VatAnimation;
};