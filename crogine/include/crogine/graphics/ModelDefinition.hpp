/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include <crogine/graphics/FontResource.hpp>
#include <crogine/graphics/MaterialResource.hpp>
#include <crogine/graphics/MeshResource.hpp>
#include <crogine/graphics/ShaderResource.hpp>
#include <crogine/graphics/TextureResource.hpp>

#include <crogine/audio/AudioResource.hpp>

#include <crogine/ecs/components/Skeleton.hpp>

#include <array>
#include <memory>

namespace cro
{
    class Entity;
    class EnvironmentMap;

    /*!
    \brief Struct of resource managers.
    Utility to pass multiple resource managers as a single parameter
    */
    struct CRO_EXPORT_API ResourceCollection final
    {
        FontResource fonts;
        MaterialResource materials;
        MeshResource meshes;
        ShaderResource shaders;
        TextureResource textures;
        AudioResource audio;
    };

    /*!
    \brief Contains all the resource IDs
    required for a specific model.
    These may be populated by loading from a path
    to a valid ConfigFile. The information can then
    be used to load data into a model component at run time.
    This is useful for loading model data from disk once and
    then creating multiple instances of it:
    \begincode
    ModelDefinition definition;
    if(definition.loadFromFile("assets/models/crate.cmt", resources))
    {
        //create the first model
        auto entity = scene.createEntity();
        entity.addComponent<Transform>().setPosition({100.f, 0.f, 100.f}):
        definition.createModel(entity, resources);

        //create a second model
        entity = scene.createEntity();
        entity.addComponent<Transform>().setPosition({40.f, 0.f, -30.f});
        definition.createModel(entity, resources);
    }
    \endcode

    Optionally, when loading models with a PBR material, a reference to an
    EnvironmentMap can be supplied to the load function so that lighting
    uniforms are automatically mapped to the material properties:

    \begincode
    if(definition.loadFromFile("assets/models/crate.cmt", resources, &environmentMap))
    {
        //create model...
    }
    \endcode

    */
    class CRO_EXPORT_API ModelDefinition final
    {
    public:
        /*!
        \brief Constructor.
        \param workingDir Set this to have resources open at a file location
        relative to a directory other than the current working directory
        */
        ModelDefinition(const std::string& workingDir = "") : m_workingDir(workingDir)
        {
            if (!workingDir.empty())
            {
                std::replace(m_workingDir.begin(), m_workingDir.end(), '\\', '/');
                if (m_workingDir.back() != '/')
                {
                    m_workingDir += '/';
                }
            }        
        }

        /*!
        \brief Attempts to load a definition from a ConfigFile at a given path.
        \param path String containing the path to a configuration file. These are
        specially formatted files containing data about a model.
        \param resources A reference to a resource collection. Resources such
        as textures and materials are loaded into this, and the ModelDefinition
        populated with IDs referring to the newly loaded data.
        \param envMap A pointer to a valid EnvironmentMap instance. This will
        automatically assign any required lighting parameters to PBR materials.
        This should be nullptr if no PBR materials are being loaded, or PBR materials
        are going to be rendered with a DeferredRenderSystem.
        \param forceReload Forces the ResourceCollection to reload any mesh data from
        file, rather than recycling any existing VBO. Generally should be false.
        \returns true if the configuration file was parsed without error.
        \see ConfigFile, EnvironmentMap
        */
        bool loadFromFile(const std::string& path, ResourceCollection& resources, EnvironmentMap* envMap = nullptr, bool forceReload = false);

        /*!
        \brief Creates a Model component from the loaded config on the given entity.
        \returns true on success, else false (no model definition has been loaded)
        Note that this may also add Skeleton components, ShadowCast components
        or BillboardCollection components necessary for a complete material, but 
        not components such as Transform, which still need to be added manually.
        \param entity A valid entity with a Transform component to which the Model
        component should be attached
        \param resources A reference to the ResourceCollection used when loading 
        the mdel definition from disk.
        */
        bool createModel(Entity entity, ResourceCollection& resoures);

        /*!
        \brief Returns true if the material for this model requested that it casts
        shadows.
        Models created with createModel() will also have a ShadowCaster components added
        if this is true.
        */
        bool castShadows() const { return m_castShadows; }

        /*!
        \brief Returns the loaded MeshID within the mesh resource supplied
        when the definition was loaded, or 0 if no mesh has been parsed
        */
        std::size_t getMeshID() const { return m_meshID; }

        /*!
        \brief Returns true if the loaded model has a skeleton.
        If this is true models created with createModel() will have a skeleton
        component added
        */
        bool hasSkeleton() const { return m_skeleton; }

    private:
        std::string m_workingDir;

        std::size_t m_meshID = 0; //!< ID of the mesh in the mesh resource
        std::array<std::int32_t, Mesh::IndexData::MaxBuffers> m_materialIDs = {}; //!< list of material IDs in the order in which they appear on the model
        std::array<std::int32_t, Mesh::IndexData::MaxBuffers> m_shadowIDs = {}; //!< IDs of shadow map materials if this model casts shadows

        std::size_t m_materialCount = 0; //!< number of active materials
        Skeleton m_skeleton; //!< overloaded operator bool indicates if currently valid
        bool m_castShadows = false; //!< if this is true the model entity also requires a shadow cast component
        bool m_billboard = false; //!< if this is true then the model is a dynamically created set of billboards

        bool m_modelLoaded = false;

        void reset();
    };
}