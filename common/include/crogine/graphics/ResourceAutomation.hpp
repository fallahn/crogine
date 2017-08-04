/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_RESOURCE_AUTO_HPP_
#define CRO_RESOURCE_AUTO_HPP_

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
    be used to load data into a model component at run time
    */
    struct CRO_EXPORT_API ModelDefinition final
    {
        int32 meshID = -1; //< ID of the mesh in the mesh resource
        std::array<int32, Mesh::IndexData::MaxBuffers> materialIDs{}; //< list of material IDs in the order in which they appear on the model
        std::array<int32, Mesh::IndexData::MaxBuffers> shadowIDs{}; //< IDs of shadow map materials if this model casts shadows
        std::size_t materialCount = 0; //< number of active materials
        std::unique_ptr<Skeleton> skeleton; //< nullptr if no skeleton exists
        bool castShadows = false; //< if this is true the model entity also requires a shadow cast component

        /*!
        \brief Attempts to load a definition from a ConfigFile at a given path.
        \param path String containing the path to a configuration file. These are
        specially formatted files containing data about a model.
        \param resources A reference to a resource collection. Resources such
        as textures and materials are loaded into this, and the ModelDefinition
        populated with IDs referring to the newly loaded data.
        \returns true if the configuration file was parsed without error.
        \see ConfigFile
        */
        bool loadFromFile(const std::string& path, ResourceCollection& resources);

        /*!
        \brief Creates a Model component from the loaded config on the given entity.
        \returns true on success, else false (no model definition has been loaded)
        */
        bool createModel(Entity, ResourceCollection&);
    };
}

#endif //CRO_RESOURCE_AUTO_HPP_