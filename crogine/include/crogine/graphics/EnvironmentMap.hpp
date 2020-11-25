/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/graphics/MaterialData.hpp>

#include <string>

/*
Parses an hdri file into a skybox, irradiance map, prefilter map and bdrf look up table
*/
namespace cro
{
    class CRO_EXPORT_API EnvironmentMap final
    {
    public:
        EnvironmentMap();

        ~EnvironmentMap();

        EnvironmentMap(const EnvironmentMap&) = delete;
        EnvironmentMap(EnvironmentMap&&) = delete;

        const EnvironmentMap& operator = (const EnvironmentMap&) = delete;
        EnvironmentMap& operator = (EnvironmentMap&&) = delete;

        bool loadFromFile(const std::string& path);

        CubemapID getSkybox() const { return CubemapID(m_skyboxTexture); }
        CubemapID getIrradianceMap() const { return CubemapID(m_irradianceTexture); }

    private:

        friend class Scene;

        std::uint32_t m_skyboxTexture;
        std::uint32_t m_irradianceTexture;


        std::uint32_t m_cubeVBO;
        std::uint32_t m_cubeVAO;
        void createCube();
        void deleteCube();
    };
}
