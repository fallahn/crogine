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

#include <crogine/graphics/ResourceAutomation.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/IqmBuilder.hpp>
#include <crogine/graphics/SphereBuilder.hpp>
#include <crogine/graphics/CubeBuilder.hpp>
#include <crogine/graphics/QuadBuilder.hpp>

#include <crogine/core/ConfigFile.hpp>
#include <crogine/util/String.hpp>

using namespace cro;

namespace
{
    std::array<std::string, 2u> materialTypes =
    {
        "VertexLit", "Unlit"
    };
}

bool ModelDefinition::loadFromFile(const std::string& path, ResourceCollection& rc)
{
    if (Util::String::getFileExtension(path) != ".cmt")
    {
        Logger::log(path + ": unusual file extention...", Logger::Type::Warning);
    }

    ConfigFile cfg;
    if (!cfg.loadFromFile(path))
    {
        Logger::log("Failed loading ModelDefinition " + path, Logger::Type::Error);
        return false;
    }

    if (Util::String::toLower(cfg.getName()) != "model")
    {
        Logger::log("No model object found in model definition " + path, Logger::Type::Error);
        return false;
    }

    auto meshPath = cfg.findProperty("mesh");
    if (!meshPath)
    {
        Logger::log(path + ": Model node contains no mesh value", Logger::Type::Error);
        return false;
    }

    const std::string& meshValue = meshPath->getValue<std::string>();
    auto ext = Util::String::getFileExtension(meshValue);
    std::unique_ptr<MeshBuilder> meshBuilder;
    bool checkSkeleton = false;
    if (ext == ".cmf")
    {
        //we have a static mesh
        meshBuilder = std::make_unique<StaticMeshBuilder>(meshValue);
    }
    else if (ext == ".iqm")
    {
        //use iqm loader
        meshBuilder = std::make_unique<IqmBuilder>(meshValue);
        checkSkeleton = true;
    }
    else if (Util::String::toLower(meshValue) == "sphere")
    {
        if (auto* prop = cfg.findProperty("radius"))
        {
            float rad = prop->getValue<float>();
            meshBuilder = std::make_unique<SphereBuilder>(rad, 8);
        }
    }
    else if (Util::String::toLower(meshValue) == "cube")
    {
        meshBuilder = std::make_unique<CubeBuilder>();
    }
    else if (Util::String::toLower(meshValue) == "quad")
    {
        glm::vec2 uv(1.f);
        if (auto* prop = cfg.findProperty("uv"))
        {
            uv = prop->getValue<glm::vec2>();
        }
        
        if (auto* prop = cfg.findProperty("size"))
        {
            glm::vec2 size = prop->getValue<glm::vec2>();
            meshBuilder = std::make_unique<QuadBuilder>(size, uv);
        }
    }
    else
    {
        //t'aint valid bruh
        Logger::log(ext + ": invalid model file type.", Logger::Type::Error);
        return false;
    }

    //check builder was created OK
    if (!meshBuilder)
    {
        Logger::log(path + ": could not create mesh builder instance", Logger::Type::Error);
        return false;
    }

    //check we have at least one material with a valid shader type
    const auto& objs = cfg.getObjects();
    std::vector<ConfigObject> materials;
    for (const auto& obj : objs)
    {
        auto type = std::find(std::begin(materialTypes), std::end(materialTypes), obj.getId());

        if (Util::String::toLower(obj.getName()) == "material"
            && type != materialTypes.end())
        {
            materials.push_back(obj);
        }
    }

    if (materials.empty())
    {
        Logger::log(path + ": no materials found.", Logger::Type::Error);
        return false;
    }

    //do all the resource loading last when we know properties are valid,
    //to prevent partially loading a model and wasting resources.
    meshID = rc.meshes.loadMesh(*meshBuilder.get());
    if (meshID == -1)
    {
        Logger::log(path + ": preloading mesh failed", Logger::Type::Error);
        return false;
    }

    if (checkSkeleton)
    {
        auto skel = dynamic_cast<IqmBuilder*>(meshBuilder.get())->getSkeleton();
        if (skel.frameCount > 0)
        {
            skeleton = std::make_unique<Skeleton>(skel);
        }
    }

    for (auto& mat : materials)
    {
        ShaderResource::BuiltIn shaderType = ShaderResource::Unlit;
        if (mat.getId() == "VertexLit") shaderType = ShaderResource::VertexLit;

        //enable shader attribs based on what the material requests
        //TODO this doesn't check valid combinations
        int32 flags = 0;
        bool smoothTextures = false;
        bool repeatTextures = false;
        const auto& properties = mat.getProperties();
        for (const auto& p : properties)
        {
            const std::string& name = Util::String::toLower(p.getName());
            if (name == "diffuse")
            {
                //diffuse map path
                if (!p.getValue<std::string>().empty())
                {
                    flags |= ShaderResource::DiffuseMap;
                }
            }
            else if (name == "normal")
            {
                //normal map path
                if (!p.getValue<std::string>().empty())
                {
                    flags |= ShaderResource::NormalMap;
                }
            }
            else if (name == "subrect")
            {
                if (!p.getValue<std::string>().empty())
                {
                    flags |= ShaderResource::Subrects;
                }
            }
            else if (name == "colour")
            {
                //colour tint
                if (!p.getValue<std::string>().empty())
                {
                    flags |= ShaderResource::DiffuseColour;
                }
            }
            else if (name == "skinned")
            {
                //model uses skinning
                if (p.getValue<bool>())
                {
                    flags |= ShaderResource::Skinning;
                }
            }
            else if (name == "vertex_coloured")
            {
                //model has vertex colours
                if (p.getValue<bool>())
                {
                    flags |= ShaderResource::VertexColour;
                }
            }
            else if (name == "projection_maps")
            {
                //receive projection maps
                if (p.getValue<bool>())
                {
                    flags |= ShaderResource::ReceiveProjection;
                }
            }
            else if (name == "smooth")
            {
                smoothTextures = p.getValue<bool>();
            }
            else if (name == "repeat")
            {
                repeatTextures = p.getValue<bool>();
            }
        }

        //load the material then check properties again for material properties
        auto shaderID = rc.shaders.preloadBuiltIn(shaderType, flags);
        auto matID = rc.materials.add(rc.shaders.get(shaderID));
        auto& material = rc.materials.get(matID);

        for (const auto& p : properties)
        {
            const auto& name = Util::String::toLower(p.getName());
            if (name == "diffuse")
            {
                auto& tex = rc.textures.get(p.getValue<std::string>());
                tex.setSmooth(smoothTextures);
                tex.setRepeated(repeatTextures);
                material.setProperty("u_diffuseMap", tex);
            }
            else if (name == "mask")
            {
                auto& tex = rc.textures.get(p.getValue<std::string>());
                tex.setSmooth(smoothTextures);
                tex.setRepeated(repeatTextures);
                material.setProperty("u_maskMap", tex);
            }
            else if (name == "normal")
            {
                auto& tex = rc.textures.get(p.getValue<std::string>());
                tex.setSmooth(smoothTextures);
                tex.setRepeated(repeatTextures);
                material.setProperty("u_normalMap", tex);
            }
            else if (name == "subrect")
            {
                material.setProperty("u_subrect", p.getValue<glm::vec4>());
            }
            else if (name == "colour")
            {
                material.setProperty("u_colour", p.getValue<glm::vec4>());
            }
            else if (name == "mask_colour")
            {
                material.setProperty("u_maskColour", p.getValue<glm::vec4>());
            }
            else if (name == "blendmode")
            {
                const auto mode = Util::String::toLower(p.getValue<std::string>());
                if (mode == "alpha")
                {
                    material.blendMode = Material::BlendMode::Alpha;
                }
                else if (mode == "add")
                {
                    material.blendMode = Material::BlendMode::Additive;
                }
                else if (mode == "multiply")
                {
                    material.blendMode = Material::BlendMode::Multiply;
                }
                //mode is None by default
            }
        }

        materialIDs[materialCount] = matID;
        materialCount++;
    }

    return true;
}