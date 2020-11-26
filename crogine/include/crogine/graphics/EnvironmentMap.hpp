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

namespace cro
{
    /*!
    An HDR (High Dynamic Range) Environment map.
    When rendering with PBR materials the PBR shader requires
    information about the environment to render correctly.

    To use an environment map with a scene make sure one instance
    exists for at least as long as any materials which reference
    it. When creating or loading a PBR material it needs three 
    properties set:

    \begincode
    material.setProperty("u_irradianceMap", environmentMap.getIrradianceMap());
    material.setProperty("u_prefilterMap", environmentMap.getPrefilterMap());
    material.setProperty("u_brdfMap", environmentMap.getBRDFMap());
    \endcode

    These properties will supply the correct environment information
    needed for PBR lighting. When using the ModelDefinition class
    this can be automated by supplying a pointer to an EnironmentMap
    as a parameter to the load function.

    EnvironmentMaps load their data from radiance *.hdr files.
    For visual feedback an environment map can be set as a Scene's
    skybox, using Scene::setCubemap()

    Note that PBR rendering, and therefore EnvironmentMaps, are not
    available on mobile hardware.

    \see ModelDefinition
    */

    class CRO_EXPORT_API EnvironmentMap final
    {
    public:
        /*!
        \brief Constructor.
        */
        EnvironmentMap();

        /*!
        \brief Destructor.
        */
        ~EnvironmentMap();

        EnvironmentMap(const EnvironmentMap&) = delete;
        EnvironmentMap(EnvironmentMap&&) = delete;

        const EnvironmentMap& operator = (const EnvironmentMap&) = delete;
        EnvironmentMap& operator = (EnvironmentMap&&) = delete;

        /*!
        \brief Attempts to load and process a radiance *.hdr file
        \param path A string containing the path to the file to load
        \returns bool - true on success, else false if loading failed.
        */
        bool loadFromFile(const std::string& path);

        /*!
        \brief Returns the texture ID for the skybox cubemap.
        This can be bound to material properties for shaders which have
        cubemap uniforms available. Note that this texture is in HDR
        not LDR (low dynamic range) format, and therefore will need
        gamma correction to render correctly.
        */
        CubemapID getSkybox() const { return CubemapID(m_textures[Skybox]); }

        /*!
        \brief Returns the cubemap texture ID for the irradiance map.
        Use this to set the irradiance map property of PBR materials:
        \beginecode
        material.setProperty("u_irradianceMap", environmentMap.getIrradianceMap());
        \endcode
        */
        CubemapID getIrradianceMap() const { return CubemapID(m_textures[Irradiance]); }

        /*!
        \brief Returns the cubemap texture ID for the prefiltered map.
        Use this to set the prefiltered map property of PBR materials:
        \beginecode
        material.setProperty("u_prefilterMap", environmentMap.getPrefilterMap());
        \endcode
        */
        CubemapID getPrefilterMap() const { return CubemapID(m_textures[Prefilter]); }

        /*!
        \brief Returns the texture ID for the BRDF map.
        Use this to set the BRDF map property of PBR materials:
        \beginecode
        material.setProperty("u_brdfMap", environmentMap.getBRDFMap());
        \endcode
        */
        TextureID getBRDFMap() const { return TextureID(m_textures[BRDF]); }

    private:

        friend class Scene;

        enum TexType
        {
            Skybox, Irradiance, Prefilter, BRDF,

            Count
        };
        std::array<std::uint32_t, TexType::Count> m_textures = {};

        void renderIrradianceMap(std::uint32_t fbo, std::uint32_t rbo, Shader&);
        void renderPrefilterMap(std::uint32_t fbo, std::uint32_t rbo, Shader&);
        void renderBRDFMap(std::uint32_t fbo, std::uint32_t rbo, Shader&);

        std::uint32_t m_cubeVBO;
        std::uint32_t m_cubeVAO;
        void createCube();
        void deleteCube();
    };
}
