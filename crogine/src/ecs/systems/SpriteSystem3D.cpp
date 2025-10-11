/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

#include "../../detail/GLCheck.hpp"

#include <crogine/ecs/systems/SpriteSystem3D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/graphics/Texture.hpp>

#include <crogine/detail/OpenGL.hpp>

#include "../../graphics/shaders/Sprite.hpp"

using namespace cro;

SpriteSystem3D::SpriteSystem3D(MessageBus& mb, float pixelsPerUnit)
    : System            (mb, typeid(SpriteSystem3D)),
    m_pixelsPerUnit     (pixelsPerUnit),
    m_meshBuilder       (std::make_unique<DynamicMeshBuilder>(VertexProperty::Position | VertexProperty::Colour | VertexProperty::UV0, 1, GL_TRIANGLES))
{
    CRO_ASSERT(pixelsPerUnit > 0, "Must be positive value");
    requireComponent<Sprite>();
    requireComponent<Model>();

    m_colouredShader.loadFromString(Shaders::Sprite::Vertex, Shaders::Sprite::Fragment);
    m_texturedShader.loadFromString(Shaders::Sprite::Vertex, Shaders::Sprite::Fragment, "#define TEXTURED\n");

    m_colouredMaterial = createMaterial(m_colouredShader);
    m_texturedMaterial = createMaterial(m_texturedShader);
}

//public
void SpriteSystem3D::process(float)
{
    //check sprites for dirty flags and update geom as necessary.
    //remember to switch shaders if a texture is added or removed.
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& sprite = entity.getComponent<Sprite>();
        if (sprite.m_dirtyFlags)
        {
            auto subRect = sprite.m_textureRect;
            FloatRect textureRect;
            if (sprite.getTexture())
            {
                textureRect = sprite.getTexture()->getNormalisedSubrect(subRect);

                //TODO we need to check if we previously had no texture and update
                //the material (definitely don't do this if we don't have to)
            }

            //TODO check the flags to see if only the colour changed
            //this requires we keep a local copy of the vertex data somewhere.
            
            subRect.width /= m_pixelsPerUnit;
            subRect.height /= m_pixelsPerUnit;

            std::vector<float> verts;

            //0------3
            //|      |
            //|      |
            //1------2

            //postion
            verts.push_back(0.f);
            verts.push_back(subRect.height);
            verts.push_back(0.f);

            //colour
            verts.push_back(sprite.getColour().getRed());
            verts.push_back(sprite.getColour().getGreen());
            verts.push_back(sprite.getColour().getBlue());
            verts.push_back(sprite.getColour().getAlpha());

            //UV
            verts.push_back(textureRect.left);
            verts.push_back(textureRect.bottom + textureRect.height);

            //------------------------

            //postion
            verts.push_back(0.f);
            verts.push_back(0.f);
            verts.push_back(0.f);

            //colour
            verts.push_back(sprite.getColour().getRed());
            verts.push_back(sprite.getColour().getGreen());
            verts.push_back(sprite.getColour().getBlue());
            verts.push_back(sprite.getColour().getAlpha());

            //UV
            verts.push_back(textureRect.left);
            verts.push_back(textureRect.bottom);

            //------------------------

           //postion
            verts.push_back(subRect.width);
            verts.push_back(0.f);
            verts.push_back(0.f);

            //colour
            verts.push_back(sprite.getColour().getRed());
            verts.push_back(sprite.getColour().getGreen());
            verts.push_back(sprite.getColour().getBlue());
            verts.push_back(sprite.getColour().getAlpha());

            //UV
            verts.push_back(textureRect.left + textureRect.width);
            verts.push_back(textureRect.bottom);

            //------------------------

           //postion
            verts.push_back(subRect.width);
            verts.push_back(subRect.height);
            verts.push_back(0.f);

            //colour
            verts.push_back(sprite.getColour().getRed());
            verts.push_back(sprite.getColour().getGreen());
            verts.push_back(sprite.getColour().getBlue());
            verts.push_back(sprite.getColour().getAlpha());

            //UV
            verts.push_back(textureRect.left + textureRect.width);
            verts.push_back(textureRect.bottom + textureRect.height);

            //update index array
            std::vector<std::uint32_t> indexData =
            {
                0, 1, 3,  3, 1, 2
            };

            auto& meshData = entity.getComponent<Model>().getMeshData();
            meshData.vertexCount = verts.size() / (meshData.vertexSize / sizeof(float));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.vboID));
            glCheck(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            meshData.indexData[0].indexCount = static_cast<std::uint32_t>(indexData.size());
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexData.size() * sizeof(std::uint32_t), indexData.data(), GL_DYNAMIC_DRAW));


            //update bounding box
            meshData.boundingBox[0] = { 0.f, 0.f, 0.1f };
            meshData.boundingBox[1] = { subRect.width,subRect.height, -0.1f };

            //update bounding sphere
            auto rad = (meshData.boundingBox[1] - meshData.boundingBox[0]) / 2.f;
            meshData.boundingSphere.centre = meshData.boundingBox[0] + rad;
            meshData.boundingSphere.radius = glm::length(rad);

            sprite.m_dirtyFlags = 0;
        }
    }
}

//private
void SpriteSystem3D::onEntityAdded(Entity entity)
{
    CRO_ASSERT(entity.getComponent<Model>().getMeshData().vboAllocation.vboID == 0, "Model data already exists!");

    //TODO take a hash of the sprite and recycle mesh data if it already exists
    //init the Model component with the mesh and corresponding material.
    auto meshData = m_meshBuilder->build(nullptr);

    //we want to make sure we copy this so it has
    //its own parameters

    Material::Data material = entity.getComponent<cro::Model>().getMaterialData(cro::Mesh::IndexData::Final, 0);
    const auto& sprite = entity.getComponent<Sprite>();
    bool attribsOK = 
    (material.attribs[Mesh::Attribute::Position][1] == 2 &&
    material.attribs[Mesh::Attribute::Colour][1] == 4 &&
    material.attribs[Mesh::Attribute::UV0][1] == 2);

    if (material.shader == 0 //no custom shader added to model
        || !attribsOK) //or the vert attribs are wrong size for our data
    {
        if (sprite.getTexture())
        {
            material = m_texturedMaterial;
            material.setProperty("u_texture", *sprite.getTexture());
        }
        else
        {
            material = m_colouredMaterial;
        }
    }

    if (sprite.m_overrideBlendMode)
    {
        material.blendMode = sprite.m_blendMode;
    }

    auto& model = entity.getComponent<cro::Model>();
    auto shadowMat = model.getMaterialData(cro::Mesh::IndexData::Shadow, 0);
    auto renderFlags = model.getRenderFlags();
    auto facing = model.getFacing();
    auto hidden = model.isHidden();

    model = Model(meshData, material);
    model.setRenderFlags(renderFlags);
    model.setFacing(facing);
    model.setHidden(hidden);

    if (entity.hasComponent<cro::ShadowCaster>())
    {
        //rebind the shadow casting material
        if (sprite.getTexture())
        {
            shadowMat.setProperty("u_diffuseMap", *sprite.getTexture());
        }
        entity.getComponent<cro::Model>().setShadowMaterial(0, shadowMat);
    }
}

void SpriteSystem3D::onEntityRemoved(Entity entity)
{
    //tidy up entity resources

    auto& meshData = entity.getComponent<Model>().getMeshData();

    //delete index buffers
    for (auto& id : meshData.indexData)
    {
        if (id.ibo)
        {
            glCheck(glDeleteBuffers(1, &id.ibo));
        }

#ifdef PLATFORM_DESKTOP
        for (auto& vao : id.vao)
        {
            if (vao)
            {
                glCheck(glDeleteVertexArrays(1, &vao));
            }
        }
#endif
    }
    //delete vertex buffer
    if (meshData.vboAllocation.vboID)
    {
        glCheck(glDeleteBuffers(1, &meshData.vboAllocation.vboID));
    }
}

Material::Data SpriteSystem3D::createMaterial(const Shader& shader)
{
    Material::Data data;
    data.shader = shader.getGLHandle();
    std::fill(data.uniforms.begin(), data.uniforms.end(), -1);

    //get the available attribs. This is sorted and culled
    //when added to a model according to the requirements of
    //the model's mesh
    const auto& shaderAttribs = shader.getAttribMap();
    for (auto i = 0u; i < shaderAttribs.size(); ++i)
    {
        data.attribs[i][Material::Data::Index] = shaderAttribs[i];
    }

    //find which uniforms exist and add them to the material
    //here we only look for the ones which we know exist in the
    //default shaders for sprites. It's up to the user to set
    //a different material on the sprite model if they want
    //something more custom.
    const auto& uniformMap = shader.getUniformMap();
    for (const auto& [uniform, handle] : uniformMap)
    {
        if (uniform == "u_worldMatrix")
        {
            data.uniforms[Material::WorldView] = handle.first;
        }
        else if (uniform == "u_viewProjectionMatrix")
        {
            data.uniforms[Material::Projection] = handle.first;
        }

        //else these are user settable uniforms - ie optional, but set by user such as textures
        else
        {
            //add to list of material properties
            data.properties.insert(std::make_pair(uniform, std::make_pair(handle.first, Material::Property())));
        }
    }
    return data;
}