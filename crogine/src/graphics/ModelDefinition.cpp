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

#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/IqmBuilder.hpp>
#include <crogine/graphics/BinaryMeshBuilder.hpp>
#include <crogine/graphics/SphereBuilder.hpp>
#include <crogine/graphics/CubeBuilder.hpp>
#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>

#include <crogine/core/ConfigFile.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/util/String.hpp>
#include <crogine/util/Maths.hpp>

#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/BillboardCollection.hpp>
#include <crogine/ecs/Entity.hpp>

using namespace cro;

namespace
{
    std::array<std::string, 4u> materialTypes =
    {
        "VertexLit", "Unlit", "Billboard", "PBR"
    };
}

ModelDefinition::ModelDefinition(ResourceCollection& rc, EnvironmentMap* envMap, const std::string& workingDir)
    : m_resources   (rc),
    m_envMap        (envMap),
    m_workingDir    (workingDir)
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

bool ModelDefinition::loadFromFile(const std::string& path, bool useDeferredShaders, bool forceReload)
{
    if (m_modelLoaded)
    {
        //cro::Logger::log("This definition already has a model loaded", cro::Logger::Type::Error);
        //return false;
        reset();
    }

    if (FileSystem::getFileExtension(path) != ".cmt")
    {
        Logger::log(path + ": unusual file extension...", Logger::Type::Warning);
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
    auto ext = FileSystem::getFileExtension(meshValue);
    std::unique_ptr<MeshBuilder> meshBuilder;

    bool lockRotation = false;
    bool lockScale = false;

    if (ext == ".cmf")
    {
        //we have a static mesh
        meshBuilder = std::make_unique<StaticMeshBuilder>(m_workingDir + meshValue);
    }
    else if (ext == ".cmb")
    {
        //binary model
        meshBuilder = std::make_unique<BinaryMeshBuilder>(m_workingDir + meshValue);
    }
    else if (ext == ".iqm")
    {
        //use iqm loader
        meshBuilder = std::make_unique<IqmBuilder>(m_workingDir + meshValue);
    }
    else if (Util::String::toLower(meshValue) == "sphere")
    {
        if (auto* prop = cfg.findProperty("radius"))
        {
            float rad = std::max(0.001f, prop->getValue<float>());
            meshBuilder = std::make_unique<SphereBuilder>(rad, 8);
        }
    }
    else if (Util::String::toLower(meshValue) == "cube")
    {
        glm::vec3 size(1.f);
        if (auto sizeProp = cfg.findProperty("size"); sizeProp)
        {
            auto sizeValue = sizeProp->getValue<glm::vec3>();
            if (sizeValue.x > 0 && sizeValue.y > 0 && sizeValue.z > 0)
            {
                size = sizeValue;
            }
        }

        meshBuilder = std::make_unique<CubeBuilder>(size);
    }
    else if (Util::String::toLower(meshValue) == "quad")
    {
        FloatRect uv(0.f, 0.f, 1.f, 1.f);
        if (auto* prop = cfg.findProperty("uv"))
        {
            uv = prop->getValue<FloatRect>();
        }
        
        if (auto* prop = cfg.findProperty("size"))
        {
            glm::vec2 size = prop->getValue<glm::vec2>();
            size.x = std::max(0.001f, size.x);
            size.y = std::max(0.001f, size.y);
            meshBuilder = std::make_unique<QuadBuilder>(size, uv);
        }
    }
    else if (Util::String::toLower(meshValue) == "billboard")
    {
        auto flags = VertexProperty::Position | VertexProperty::Normal | VertexProperty::Colour | VertexProperty::UV0 | VertexProperty::UV1;
        meshBuilder = std::make_unique<DynamicMeshBuilder>(flags, 1, GL_TRIANGLES);
        m_billboard = true;

        if (auto* prop = cfg.findProperty("lock_rotation"); prop != nullptr)
        {
            lockRotation = prop->getValue<bool>();
        }

        if (auto* prop = cfg.findProperty("lock_scale"); prop != nullptr)
        {
            lockScale = prop->getValue<bool>();
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

    //check to see if this model ought to cast shadows
    auto shadowProp = cfg.findProperty("cast_shadows");
    if (shadowProp)
    {
        m_castShadows = shadowProp->getValue<bool>();
    }

    //do all the resource loading last when we know properties are valid,
    //to prevent partially loading a model and wasting resources.
    m_meshID = m_resources.meshes.loadMesh(*meshBuilder.get(), forceReload);
    if (m_meshID == 0)
    {
        Logger::log(path + ": preloading mesh failed", Logger::Type::Error);
        return false;
    }

    auto skel = m_resources.meshes.getSkeltalAnimation(m_meshID);
    if (skel)
    {
        m_skeleton = skel;
    }

    for (auto& mat : materials)
    {
        ShaderResource::BuiltIn shaderType = useDeferredShaders ? ShaderResource::UnlitDeferred : ShaderResource::Unlit;
        if (mat.getId() == "VertexLit")
        {
            if (m_billboard)
            {
                shaderType = ShaderResource::BillboardVertexLit;
            }
            else
            {
                shaderType = useDeferredShaders ? ShaderResource::VertexLitDeferred : ShaderResource::VertexLit;
            }
        }
        else if (mat.getId() == "PBR")
        {
            if (m_billboard)
            {
                LogE << "PBR materials cannot currently be used on billboard meshes." << std::endl;
                shaderType = ShaderResource::BillboardVertexLit;
            }
            else
            {
                shaderType = useDeferredShaders ? ShaderResource::PBRDeferred : ShaderResource::PBR;
            }
        }
        else if (m_billboard)
        {
            shaderType = ShaderResource::BillboardUnlit;
        }
         

        //enable shader attribs based on what the material requests
        //TODO this doesn't check valid combinations
        std::int32_t flags = 0;
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
            else if (name == "mask")
            {
                if (!p.getValue<std::string>().empty())
                {
                    flags |= ShaderResource::MaskMap;
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
            else if (name == "rim")
            {
                //if (p.getValue<glm::vec4>().a == 1)
                {
                    flags |= ShaderResource::RimLighting;
                }
            }
            else if (name == "projection")
            {
                //receive projection maps
                if (!p.getValue<std::string>().empty())
                {
                    flags |= ShaderResource::ReceiveProjection;
                }
            }
            else if (name == "lightmap")
            {
                if (!p.getValue<std::string>().empty())
                {
                    flags |= ShaderResource::LightMap;
                }
            }
            else if (name == "rx_shadows")
            {
                if (p.getValue<bool>())
                {
                    flags |= ShaderResource::RxShadows;
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
            else if (name == "alpha_clip")
            {
                flags |= ShaderResource::AlphaClip;
            }
        }

        if (lockRotation)
        {
            flags |= ShaderResource::BuiltInFlags::LockRotation;
        }

        if (lockScale)
        {
            flags |= ShaderResource::BuiltInFlags::LockScale;
        }

        if (m_billboard)
        {
            flags |= ShaderResource::BuiltInFlags::VertexColour;
        }

        //load the material then check properties again for material properties
        auto shaderID = m_resources.shaders.loadBuiltIn(shaderType, flags);
        auto matID = m_resources.materials.add(m_resources.shaders.get(shaderID));
        auto& material = m_resources.materials.get(matID);
        material.deferred = shaderType == ShaderResource::PBRDeferred;

        //set a default mask colour - this is overwritten
        //below, if a custom property is found.
        if ((flags & ShaderResource::MaskMap) == 0)
        {
            if (shaderType == ShaderResource::VertexLit
                || shaderType == ShaderResource::VertexLitDeferred)
            {
                material.setProperty("u_maskColour", cro::Colour::Yellow);
            }
            else if (shaderType == ShaderResource::PBR
                || shaderType == ShaderResource::PBRDeferred)
            {
                material.setProperty("u_maskColour", cro::Colour::Blue);
            }
        }

        //store the diffuse and alpha clip properties in case
        //they need to be set on the shadow map material.
        Texture* diffuseTex = nullptr;
        float alphaClip = 0.f;

        for (const auto& p : properties)
        {
            const auto& name = Util::String::toLower(p.getName());
            if (name == "diffuse")
            {
                auto& tex = m_resources.textures.get(m_workingDir + p.getValue<std::string>());
                tex.setSmooth(smoothTextures);
                tex.setRepeated(repeatTextures);
                material.setProperty("u_diffuseMap", tex);

                diffuseTex = &tex;
            }
            else if (name == "mask")
            {
                auto& tex = m_resources.textures.get(m_workingDir + p.getValue<std::string>());
                tex.setSmooth(smoothTextures);
                tex.setRepeated(repeatTextures);
                material.setProperty("u_maskMap", tex);
            }
            else if (name == "normal")
            {
                auto& tex = m_resources.textures.get(m_workingDir + p.getValue<std::string>());
                tex.setSmooth(smoothTextures);
                tex.setRepeated(repeatTextures);
                material.setProperty("u_normalMap", tex);
            }
            else if (name == "lightmap")
            {
                auto& tex = m_resources.textures.get(m_workingDir + p.getValue<std::string>());
                tex.setSmooth(true);
                material.setProperty("u_lightMap", tex);
            }
            else if (name == "rim")
            {
                auto c = p.getValue<glm::vec4>();
                material.setProperty("u_rimColour", 
                    Colour(
                        Util::Maths::clamp(c.r, 0.f, 1.f), 
                        Util::Maths::clamp(c.g, 0.f, 1.f),
                        Util::Maths::clamp(c.b, 0.f, 1.f),
                        Util::Maths::clamp(c.a, 0.f, 1.f)));

                if (auto* rimProperty = mat.findProperty("rim_falloff"))
                {
                    material.setProperty("u_rimFalloff", Util::Maths::clamp(rimProperty->getValue<float>(), 0.1f, 1.f));
                }
                else
                {
                    material.setProperty("u_rimFalloff", 0.6f);
                }
            }
            else if (name == "projection")
            {
                auto& tex = m_resources.textures.get(p.getValue<std::string>(), false);
                tex.setSmooth(smoothTextures);
                material.setProperty("u_projectionMap", tex);
            }
            else if (name == "subrect")
            {
                auto subrect = p.getValue<glm::vec4>();
                auto clamp = [](float& v)
                {
                    v = std::min(1.f, std::max(0.f, v));
                };
                clamp(subrect.x);
                clamp(subrect.y);
                clamp(subrect.z);
                clamp(subrect.w);

                material.setProperty("u_subrect", subrect);
            }
            else if (name == "colour")
            {
                auto c = p.getValue<glm::vec4>();
                material.setProperty("u_colour", Colour(
                    Util::Maths::clamp(c.r, 0.f, 1.f),
                    Util::Maths::clamp(c.g, 0.f, 1.f),
                    Util::Maths::clamp(c.b, 0.f, 1.f),
                    Util::Maths::clamp(c.a, 0.f, 1.f)));
            }
            else if (name == "mask_colour")
            {
                auto c = p.getValue<glm::vec4>();
                material.setProperty("u_maskColour", Colour(
                    Util::Maths::clamp(c.r, 0.f, 1.f),
                    Util::Maths::clamp(c.g, 0.f, 1.f),
                    Util::Maths::clamp(c.b, 0.f, 1.f),
                    Util::Maths::clamp(c.a, 0.f, 1.f)));

                if (shaderType == ShaderResource::PBR
                    && c.b < 1)
                {
                    LogW << "PBR Material has an AO mask value less than one. Is this intentional? " << std::endl;
                }
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
            else if (name == "alpha_clip")
            {
                alphaClip = Util::Maths::clamp(p.getValue<float>(), 0.f, 1.f);
                material.setProperty("u_alphaClip", alphaClip);
            }
        }

        //check to see if we can map environment lighting
        if (shaderType == ShaderResource::PBR)
        {
            CRO_ASSERT(m_envMap, "No environment map was pass on construction");
            material.setProperty("u_irradianceMap", m_envMap->getIrradianceMap());
            material.setProperty("u_prefilterMap", m_envMap->getPrefilterMap());
            material.setProperty("u_brdfMap", m_envMap->getBRDFMap());
        }

        m_materialIDs[m_materialCount] = matID;

        if (m_castShadows)
        {
            flags = ShaderResource::DepthMap | (flags & (ShaderResource::Skinning | ShaderResource::AlphaClip));

            shaderID = m_resources.shaders.loadBuiltIn(m_billboard ? ShaderResource::BillboardShadowMap : ShaderResource::ShadowMap, flags);
            matID = m_resources.materials.add(m_resources.shaders.get(shaderID));
            m_shadowIDs[m_materialCount] = matID;

            if (flags & ShaderResource::AlphaClip
                && diffuseTex != nullptr)
            {
                auto& m = m_resources.materials.get(matID);
                m.setProperty("u_diffuseMap", *diffuseTex);
                m.setProperty("u_alphaClip", alphaClip);
            }
        }

        m_materialCount++;
    }

    m_modelLoaded = true;
    return true;
}

bool ModelDefinition::createModel(Entity entity)
{
    CRO_ASSERT(entity.isValid(), "Invalid Entity");
    CRO_ASSERT(entity.hasComponent<cro::Transform>(), "Missing transform component");

    if (m_meshID != 0)
    {
        auto& model = entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_meshID), m_resources.materials.get(m_materialIDs[0]));
        for (auto i = 1u; i < m_materialCount; ++i)
        {
            model.setMaterial(i, m_resources.materials.get(m_materialIDs[i]));
        }

        if (m_castShadows)
        {
            //while this technically works the nature of billboards
            //mean that the output would be facing the camera in the
            //depth map view, which is incorrect - in this pass we'd
            //still need to supply the viewProj for the active camera
            //not the light source.
            if (!m_billboard)
            {
                for (auto i = 0u; i < m_materialCount; ++i)
                {
                    if (m_shadowIDs[i] > 0)
                    {
                        model.setShadowMaterial(i, m_resources.materials.get(m_shadowIDs[i]));
                    }
                }
                entity.addComponent<ShadowCaster>().skinned = (m_skeleton);
            }
            else
            {
                LogW << "Billboard materials do not support shadow casting, property is ignored." << std::endl;
            }
        }

        if (m_billboard)
        {
            entity.addComponent<BillboardCollection>();
            LOG("Multiple billboards created from the same definition will share the same mesh data!", cro::Logger::Type::Warning);
        }

        if (hasSkeleton())
        {
            entity.addComponent<cro::Skeleton>() = m_skeleton;
        }

        return true;
    }
    return false;
}

void ModelDefinition::reset()
{
    m_meshID = 0;
    m_materialIDs = {};
    m_shadowIDs = {};
    m_materialCount = 0;
    Skeleton m_skeleton = {};
    m_castShadows = false;

    m_modelLoaded = false;
}