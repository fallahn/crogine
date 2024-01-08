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

#include "ModelState.hpp"
#include "SharedStateData.hpp"

#include <crogine/ecs/components/Model.hpp>

#include <crogine/util/String.hpp>

#include <functional>

std::uint32_t ModelState::addTextureToBrowser(const std::string& path)
{
    auto fileName = cro::FileSystem::getFileName(path);

    auto relPath = path;
    std::replace(relPath.begin(), relPath.end(), '\\', '/');

    std::hash<std::string> hashAttack;
    auto uid = hashAttack(path);

    relPath = cro::FileSystem::getFilePath(relPath);
    relPath = cro::FileSystem::getRelativePath(relPath, m_sharedData.workingDirectory);

    //replace if exists
    std::uint32_t id = 0;
    for (auto& [i, t] : m_materialTextures)
    {
        if (t.uid == uid)
        {
            id = i;
            break;
        }
    }

    bool updateMaterials = false;
    if (id != 0)
    {
        m_materialTextures.erase(id);
        updateMaterials = true;
    }
    m_selectedTexture = 0;


    std::uint32_t retVal = 0;

    MaterialTexture tex;
    tex.texture = std::make_unique<cro::Texture>();
    if (tex.texture->loadFromFile(path))
    {
        //TODO we should be reading the smooth/repeat property from the material file
        tex.texture->setSmooth(true);

        m_selectedTexture = tex.texture->getGLHandle();
        tex.name = fileName;
        tex.relPath = relPath;
        tex.uid = uid;
        m_materialTextures.insert(std::make_pair(tex.texture->getGLHandle(), std::move(tex)));
        retVal = m_selectedTexture;
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed to open image", cro::FileSystem::OK, cro::FileSystem::Error);
    }

    if (updateMaterials)
    {
        for (auto& mat : m_materialDefs)
        {
            for (auto& texID : mat.textureIDs)
            {
                if (texID == id)
                {
                    texID = retVal;
                }
            }
        }
    }

    return retVal;
}

void ModelState::addMaterialToBrowser(MaterialDefinition&& def)
{
    //update the thumbnail
    refreshMaterialThumbnail(def);

    m_materialDefs.push_back(std::move(def));
    m_selectedMaterial = m_materialDefs.size() - 1;
}

void ModelState::applyPreviewSettings(MaterialDefinition& matDef)
{
    if (matDef.textureIDs[MaterialDefinition::Diffuse])
    {
        auto& tex = m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Diffuse]).texture;

        matDef.materialData.setProperty("u_diffuseMap", *tex);
        tex->setRepeated(matDef.repeatTexture);
        tex->setSmooth(matDef.smoothTexture);

        if (matDef.alphaClip > 0)
        {
            matDef.materialData.setProperty("u_alphaClip", matDef.alphaClip);
        }
    }

    if (matDef.textureIDs[MaterialDefinition::Mask]
        && matDef.type != MaterialDefinition::Unlit)
    {
        auto& tex = m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Mask]).texture;
        matDef.materialData.setProperty("u_maskMap", *tex);
        tex->setSmooth(matDef.smoothTexture);
        tex->setRepeated(matDef.repeatTexture);
    }
    else if (matDef.type != MaterialDefinition::Unlit)
    {
        matDef.materialData.setProperty("u_maskColour", matDef.maskColour);
    }

    if (matDef.textureIDs[MaterialDefinition::Normal])
    {
        auto& tex = m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Normal]).texture;
        matDef.materialData.setProperty("u_normalMap", *tex);
        tex->setSmooth(matDef.smoothTexture);
        tex->setRepeated(matDef.repeatTexture);
    }

    if (matDef.textureIDs[MaterialDefinition::Lightmap]
        && matDef.type != MaterialDefinition::PBR)
    {
        auto& tex = m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Lightmap]).texture;
        matDef.materialData.setProperty("u_lightMap", *tex);
        tex->setRepeated(matDef.repeatTexture);
        tex->setSmooth(matDef.smoothTexture);
    }

    //if (matDef.shaderFlags & cro::ShaderResource::DiffuseColour)
    {
        matDef.materialData.setProperty("u_colour", matDef.colour);
    }

    if (matDef.useRimlighing
        && matDef.type != MaterialDefinition::PBR)
    {
        matDef.materialData.setProperty("u_rimColour", matDef.rimlightColour);
        matDef.materialData.setProperty("u_rimFalloff", matDef.rimlightFalloff);
    }

    if (matDef.useSubrect)
    {
        matDef.materialData.setProperty("u_subrect", matDef.subrect);
    }
    else if (matDef.animated)
    {
        matDef.materialData.animation.frame.x = 0.f;
        matDef.materialData.animation.frame.z = 1.f / matDef.colCount;

        matDef.materialData.animation.frame.w = 1.f / matDef.rowCount;
        matDef.materialData.animation.frame.y = 1.f - matDef.materialData.animation.frame.w;

        matDef.materialData.animation.frameTime = 1.f / matDef.frameRate;

        matDef.materialData.setProperty("u_subrect", matDef.materialData.animation.frame);
    }

    matDef.materialData.animation.active = matDef.animated;
    matDef.materialData.blendMode = matDef.blendMode;
    matDef.materialData.enableDepthTest = matDef.depthTest;
    matDef.materialData.doubleSided = matDef.doubleSided;

    if (matDef.type == MaterialDefinition::PBR)
    {
        matDef.materialData.setProperty("u_irradianceMap", m_environmentMap.getIrradianceMap());
        matDef.materialData.setProperty("u_prefilterMap", m_environmentMap.getPrefilterMap());
        matDef.materialData.setProperty("u_brdfMap", m_environmentMap.getBRDFMap());
    }
}

void ModelState::refreshMaterialThumbnail(MaterialDefinition& def)
{
    def.shaderFlags = 0;

    auto shaderType = m_useDeferred ? cro::ShaderResource::UnlitDeferred : cro::ShaderResource::Unlit;
    if (m_modelProperties.type == ModelProperties::Billboard)
    {
        shaderType = cro::ShaderResource::BillboardUnlit;
    }
    if (def.type == MaterialDefinition::VertexLit)
    {
        shaderType = m_useDeferred ? cro::ShaderResource::VertexLitDeferred : cro::ShaderResource::VertexLit;
        if (m_modelProperties.type == ModelProperties::Billboard)
        {
            shaderType = cro::ShaderResource::BillboardVertexLit;
        }
    }
    else if (def.type == MaterialDefinition::PBR)
    {
        shaderType = m_useDeferred ? cro::ShaderResource::PBRDeferred : cro::ShaderResource::PBR;
    }

    if (def.type != MaterialDefinition::Unlit &&
        def.textureIDs[MaterialDefinition::Normal])
    {
        def.shaderFlags |= cro::ShaderResource::NormalMap;
    }


    if (def.type != MaterialDefinition::PBR &&
        def.textureIDs[MaterialDefinition::Lightmap])
    {
        def.shaderFlags |= cro::ShaderResource::LightMap;
    }

    if (def.textureIDs[MaterialDefinition::Mask])
    {
        def.shaderFlags |= cro::ShaderResource::MaskMap;
    }

    if (def.textureIDs[MaterialDefinition::Diffuse])
    {
        def.shaderFlags |= cro::ShaderResource::DiffuseMap;
    }
    //else
    {
        def.shaderFlags |= cro::ShaderResource::DiffuseColour;
    }

    if (def.alphaClip > 0)
    {
        def.shaderFlags |= cro::ShaderResource::AlphaClip;
    }

    if (def.vertexColoured)
    {
        def.shaderFlags |= cro::ShaderResource::VertexColour;
    }

    if (def.receiveShadows)
    {
        def.shaderFlags |= cro::ShaderResource::RxShadows;
    }

    if (def.useRimlighing)
    {
        def.shaderFlags |= cro::ShaderResource::RimLighting;
    }

    if (def.useSubrect || def.animated)
    {
        def.shaderFlags |= cro::ShaderResource::Subrects;
    }

    if (def.skinned)
    {
        def.shaderFlags |= cro::ShaderResource::Skinning;
    }

    def.shaderID = m_resources.shaders.loadBuiltIn(shaderType, def.shaderFlags);
    def.materialData.setShader(m_resources.shaders.get(def.shaderID));
    def.materialData.deferred = (shaderType == cro::ShaderResource::PBRDeferred);
    def.materialData.enableDepthTest = def.depthTest;
    def.materialData.doubleSided = def.doubleSided;

    applyPreviewSettings(def);
    m_previewEntity.getComponent<cro::Model>().setMaterial(0, def.materialData);

    def.previewTexture.clear(uiConst::PreviewClearColour);
    m_previewScene.render();
    def.previewTexture.display();
}

void ModelState::exportMaterial() const
{
    auto defaultName = m_materialDefs[m_selectedMaterial].name;
    if (defaultName.empty())
    {
        defaultName = "untitled";
    }
    else
    {
        std::replace(defaultName.begin(), defaultName.end(), ' ', '_');
        defaultName = cro::Util::String::toLower(defaultName);
    }
    auto path = cro::FileSystem::saveFileDialogue(m_sharedData.workingDirectory + "/" + defaultName, "mdf");
    if (!path.empty())
    {
        if (cro::FileSystem::getFileExtension(path) != ".mdf")
        {
            path += ".mdf";
        }

        const auto& matDef = m_materialDefs[m_selectedMaterial];
        auto name = matDef.name;
        std::replace(name.begin(), name.end(), ' ', '_');

        /*
        REMEMBER if we add more properties here they also need to
        be added to ModelState::saveModel()
        */

        cro::ConfigFile file("material_definition", name);
        file.addProperty("type").setValue(matDef.type);
        file.addProperty("colour").setValue(matDef.colour.getVec4());
        file.addProperty("mask_colour").setValue(matDef.maskColour.getVec4());
        file.addProperty("alpha_clip").setValue(matDef.alphaClip);
        file.addProperty("vertex_coloured").setValue(matDef.vertexColoured);
        file.addProperty("rx_shadow").setValue(matDef.receiveShadows);
        file.addProperty("blend_mode").setValue(static_cast<std::int32_t>(matDef.blendMode));
        file.addProperty("use_rimlight").setValue(matDef.useRimlighing);
        file.addProperty("rimlight_falloff").setValue(matDef.rimlightFalloff);
        file.addProperty("rimlight_colour").setValue(matDef.rimlightColour);
        file.addProperty("use_subrect").setValue(matDef.useSubrect);
        file.addProperty("subrect").setValue(matDef.subrect);
        file.addProperty("texture_smooth").setValue(matDef.smoothTexture);
        file.addProperty("texture_repeat").setValue(matDef.repeatTexture);
        file.addProperty("depth_test").setValue(matDef.depthTest);
        file.addProperty("double_sided").setValue(matDef.doubleSided);
        file.addProperty("use_mipmaps").setValue(matDef.useMipmaps);
        file.addProperty("animated").setValue(matDef.animated);
        file.addProperty("row_count").setValue(matDef.rowCount);
        file.addProperty("col_count").setValue(matDef.colCount);
        file.addProperty("framerate").setValue(matDef.frameRate);

        //textures
        auto getTextureName = [&](std::uint32_t id)
        {
            const auto& t = m_materialTextures.at(id);
            return t.relPath + t.name;
        };

        if (matDef.textureIDs[MaterialDefinition::Diffuse] != 0)
        {
            file.addProperty("diffuse", getTextureName(matDef.textureIDs[MaterialDefinition::Diffuse]));
        }

        if (matDef.textureIDs[MaterialDefinition::Mask] != 0)
        {
            file.addProperty("mask", getTextureName(matDef.textureIDs[MaterialDefinition::Mask]));
        }

        if (matDef.textureIDs[MaterialDefinition::Normal] != 0)
        {
            file.addProperty("normal", getTextureName(matDef.textureIDs[MaterialDefinition::Normal]));
        }

        if (matDef.textureIDs[MaterialDefinition::Lightmap] != 0)
        {
            file.addProperty("lightmap", getTextureName(matDef.textureIDs[MaterialDefinition::Lightmap]));
        }

        if(!matDef.tags.empty())
        {
            auto* o = file.addObject("tags");
            for (const auto& tag : matDef.tags)
            {
                o->addProperty("tag").setValue(tag);
            }
        }

        file.save(path);
    }
}

void ModelState::importMaterial(const std::string& path)
{
    cro::ConfigFile file;
    auto extension = cro::FileSystem::getFileExtension(path);
    if (extension == ".mdf")
    {
        if (file.loadFromFile(path))
        {
            auto name = file.getId();
            std::replace(name.begin(), name.end(), '_', ' ');

            MaterialDefinition def;
            def.materialData = m_resources.materials.get(m_materialIDs[MaterialID::Default]);

            if (!name.empty())
            {
                def.name = name;
            }

            const auto& properties = file.getProperties();
            for (const auto& prop : properties)
            {
                name = prop.getName();
                if (name == "type")
                {
                    auto val = prop.getValue<std::int32_t>();
                    if (val > -1 && val < MaterialDefinition::Count)
                    {
                        def.type = static_cast<MaterialDefinition::Type>(val);
                    }
                }
                else if (name == "colour")
                {
                    def.colour = prop.getValue<cro::Colour>();
                }
                else if (name == "mask_colour")
                {
                    def.maskColour = prop.getValue<cro::Colour>();
                }
                else if (name == "alpha_clip")
                {
                    def.alphaClip = std::max(0.f, std::min(1.f, prop.getValue<float>()));
                }
                else if (name == "vertex_coloured")
                {
                    def.vertexColoured = prop.getValue<bool>();
                }
                else if (name == "rx_shadow")
                {
                    def.receiveShadows = prop.getValue<bool>();
                }
                else if (name == "blend_mode")
                {
                    auto val = prop.getValue<std::int32_t>();
                    if (val > -1 && val <= static_cast<std::int32_t>(cro::Material::BlendMode::Additive))
                    {
                        def.blendMode = static_cast<cro::Material::BlendMode>(val);
                    }
                }
                else if (name == "diffuse")
                {
                    def.textureIDs[MaterialDefinition::Diffuse] = addTextureToBrowser(m_sharedData.workingDirectory + "/" + prop.getValue<std::string>());
                    if (def.textureIDs[MaterialDefinition::Diffuse] == 0)
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed opening texture. Check the working directory is set (View->Options)");
                    }
                }
                else if (name == "mask")
                {
                    def.textureIDs[MaterialDefinition::Mask] = addTextureToBrowser(m_sharedData.workingDirectory + "/" + prop.getValue<std::string>());
                    if (def.textureIDs[MaterialDefinition::Mask] == 0)
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed opening texture. Check the working directory is set (View->Options)");
                    }
                }
                else if (name == "lightmap")
                {
                    def.textureIDs[MaterialDefinition::Lightmap] = addTextureToBrowser(m_sharedData.workingDirectory + "/" + prop.getValue<std::string>());
                    if (def.textureIDs[MaterialDefinition::Lightmap] == 0)
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed opening texture. Check the working directory is set (View->Options)");
                    }
                }
                else if (name == "normal")
                {
                    def.textureIDs[MaterialDefinition::Normal] = addTextureToBrowser(m_sharedData.workingDirectory + "/" + prop.getValue<std::string>());
                    if (def.textureIDs[MaterialDefinition::Normal] == 0)
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed opening texture. Check the working directory is set (View->Options)");
                    }
                }
                else if (name == "use_rimlight")
                {
                    def.useRimlighing = prop.getValue<bool>();
                }
                else if (name == "rimlight_colour")
                {
                    def.rimlightColour = prop.getValue<cro::Colour>();
                }
                else if (name == "rimlight_falloff")
                {
                    def.rimlightFalloff = std::min(1.f, std::max(0.f, prop.getValue<float>()));
                }
                else if (name == "use_subrect")
                {
                    def.useSubrect = prop.getValue<bool>();
                }
                else if (name == "subrect")
                {
                    auto subrect = prop.getValue<glm::vec4>();
                    const auto clamp = [](float& v)
                    {
                        v = std::min(1.f, std::max(0.f, v));
                    };
                    clamp(subrect.x);
                    clamp(subrect.y);
                    clamp(subrect.z);
                    clamp(subrect.w);
                    def.subrect = subrect;
                }
                else if (name == "texture_smooth")
                {
                    def.smoothTexture = prop.getValue<bool>();
                }
                else if (name == "texture_repeat")
                {
                    def.repeatTexture = prop.getValue<bool>();
                }
                else if (name == "depth_test")
                {
                    def.depthTest = prop.getValue<bool>();
                }
                else if (name == "double_sided")
                {
                    def.doubleSided = prop.getValue<bool>();
                }
                else if (name == "use_mipmaps")
                {
                    def.useMipmaps = prop.getValue<bool>();
                }
            }

            //sub-objects (currently just nested tags)
            const auto& subObjs = file.getObjects();
            for (const auto& so : subObjs)
            {
                if (so.getName() == "tags")
                {
                    const auto& subProps = so.getProperties();
                    for (const auto& sp : subProps)
                    {
                        if (sp.getName() == "tag")
                        {
                            def.tags.push_back(sp.getValue<std::string>());
                        }
                    }
                }
            }

            addMaterialToBrowser(std::move(def));
        }
    }
    else if (extension == ".cmt")
    {
        if (file.loadFromFile(path))
        {
            //this is a regular model def so try parsing the materials
            const auto& objects = file.getObjects();
            for (const auto& obj : objects)
            {
                if (obj.getName() == "material")
                {
                    MaterialDefinition matDef;
                    readMaterialDefinition(matDef, obj);

                    addMaterialToBrowser(std::move(matDef));
                }
            }
        }
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Could not open material file.");
    }
}

void ModelState::readMaterialDefinition(MaterialDefinition& matDef, const cro::ConfigObject& obj)
{
    std::string_view id = obj.getId();
    if (id == "Unlit")
    {
        matDef.type = MaterialDefinition::Unlit;
    }
    else if (id == "VertexLit")
    {
        matDef.type = MaterialDefinition::VertexLit;
    }
    else if (id == "PBR")
    {
        matDef.type = MaterialDefinition::PBR;
    }

    //read textures
    const auto& properties = obj.getProperties();
    for (const auto& prop : properties)
    {
        std::string_view name = prop.getName();
        if (name == "diffuse"
            || name == "mask"
            || name == "normal"
            || name == "lightmap")
        {
            auto path = m_sharedData.workingDirectory + "/" + prop.getValue<std::string>();

            //TODO use this to correct path for textures which are in the same dir as definition
            /*
            auto pos = filePath.find_last_of('/');
            if (pos == std::string::npos)
            {
                pos = path.find_last_of('/');
                if (pos != std::string::npos)
                {
                    filePath = path.substr(0, path.find_last_of('/')) + "/" + filePath;
                }
                else
                {
                    filePath = m_workingDir + filePath;
                }
            }
            else
            {
                filePath = m_workingDir + filePath;
            }
            */
            LogI << __FILE__ << ", " << __LINE__ << "Fix me" << std::endl;

            if (name == "diffuse") matDef.textureIDs[MaterialDefinition::Diffuse] = addTextureToBrowser(path);
            if (name == "mask") matDef.textureIDs[MaterialDefinition::Mask] = addTextureToBrowser(path);
            if (name == "normal") matDef.textureIDs[MaterialDefinition::Normal] = addTextureToBrowser(path);
            if (name == "lightmap") matDef.textureIDs[MaterialDefinition::Lightmap] = addTextureToBrowser(path);
        }
        else if (name == "colour")
        {
            matDef.colour = prop.getValue<cro::Colour>();
        }
        else if (name == "vertex_coloured")
        {
            matDef.vertexColoured = prop.getValue<bool>();
        }
        else if (name == "rim")
        {
            matDef.useRimlighing = true;
            matDef.rimlightColour = prop.getValue<cro::Colour>();
        }
        else if (name == "rim_falloff")
        {
            matDef.useRimlighing = true;
            matDef.rimlightFalloff = prop.getValue<float>();
        }
        else if (name == "rx_shadows")
        {
            matDef.receiveShadows = prop.getValue<bool>();
        }
        else if (name == "blendmode")
        {
            auto mode = prop.getValue<std::string>();
            if (mode == "alpha")
            {
                matDef.blendMode = cro::Material::BlendMode::Alpha;
            }
            else if (mode == "add")
            {
                matDef.blendMode = cro::Material::BlendMode::Additive;
            }
            else if (mode == "multiply")
            {
                matDef.blendMode = cro::Material::BlendMode::Multiply;
            }
        }
        else if (name == "alpha_clip")
        {
            matDef.alphaClip = prop.getValue<float>();
        }
        else if (name == "mask_colour")
        {
            matDef.maskColour = prop.getValue<cro::Colour>();
        }
        else if (name == "name")
        {
            auto val = prop.getValue<std::string>();
            if (!val.empty())
            {
                matDef.name = val;
            }
        }
        else if (name == "subrect")
        {
            matDef.useSubrect = true;
            auto subrect = prop.getValue<glm::vec4>();
            auto clamp = [](float& v)
            {
                v = std::min(1.f, std::max(0.f, v));
            };
            clamp(subrect.x);
            clamp(subrect.y);
            clamp(subrect.z);
            clamp(subrect.w);
            matDef.subrect = subrect;
        }
        else if (name == "skinned")
        {
            matDef.skinned = prop.getValue<bool>();
        }
        else if (name == "smooth")
        {
            matDef.smoothTexture = prop.getValue<bool>();
        }
        else if (name == "repeat")
        {
            matDef.repeatTexture = prop.getValue<bool>();
        }
        else if (name == "depth_test")
        {
            matDef.depthTest = prop.getValue<bool>();
        }
        else if (name == "double_sided")
        {
            matDef.doubleSided = prop.getValue<bool>();
        }
        else if (name == "animated")
        {
            matDef.animated = prop.getValue<bool>();
        }
        else if (name == "row_count")
        {
            matDef.rowCount = std::max(1u, prop.getValue<std::uint32_t>());
        }
        else if (name == "col_count")
        {
            matDef.colCount = std::max(1u, prop.getValue<std::uint32_t>());
        }
        else if (name == "framerate")
        {
            matDef.frameRate = std::max(1.f, prop.getValue<float>());
        }
        else if (name == "use_mipmaps")
        {
            matDef.useMipmaps = prop.getValue<bool>();
        }
    }

    //sub-objects (currently just nested tags)
    const auto& subObjs = obj.getObjects();
    for (const auto& so : subObjs)
    {
        if (so.getName() == "tags")
        {
            const auto& subProps = so.getProperties();
            for (const auto& sp : subProps)
            {
                if (sp.getName() == "tag")
                {
                    matDef.tags.push_back(sp.getValue<std::string>());
                }
            }
        }
    }
}