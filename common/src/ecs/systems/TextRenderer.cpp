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

#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include "../../graphics/shaders/Sprite.hpp"
#include "../../detail/GLCheck.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace cro;

namespace
{
    uint32 MaxTexts = 127u; //this can be as low as 63 on mobile or high as 511
    constexpr uint32 vertexSize = (4 + 4 + 2 + 2) * sizeof(float); //pos, colour, UV0, UV1
}

TextRenderer::TextRenderer(MessageBus& mb)
    : System                (mb, typeid(TextRenderer)),
    m_pendingRebuild        (false)
{
    GLint maxVec;
    glCheck(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVec));
    MaxTexts = maxVec / 4; //4 x 4-components make up a mat4.
    MaxTexts = std::min(MaxTexts - 1, 255u); //caps size on platforms such as VMs which incorrectly report max_vectors
    LOG(std::to_string(MaxTexts) + " texts are available per batch", Logger::Type::Info);

    if (!m_shaders[Font::Type::Bitmap].shader.loadFromString(Shaders::Sprite::Vertex, Shaders::Text::BitmapFragment, "#define MAX_MATRICES " + std::to_string(MaxTexts) + "\n"))
    {
        Logger::log("Failed loading bitmap font shader, text renderer is in invalid state", Logger::Type::Error, Logger::Output::All);
    }

    if (!m_shaders[Font::Type::SDF].shader.loadFromString(Shaders::Sprite::Vertex, Shaders::Text::SDFFragment, "#define MAX_MATRICES " + std::to_string(MaxTexts) + "\n"))
    {
        Logger::log("Failed loading SDF font shader, text renderer is in invalid state", Logger::Type::Error, Logger::Output::All);
    }
    fetchShaderData(m_shaders[Font::Type::Bitmap]);
    fetchShaderData(m_shaders[Font::SDF]);

    requireComponent<Text>();
    requireComponent<Transform>();
}

TextRenderer::~TextRenderer()
{
    for (auto b : m_buffers)
    {
        glCheck(glDeleteBuffers(1, &b.first));
    }
}

//public
void TextRenderer::handleMessage(const Message& msg)
{

}

void TextRenderer::process(Time dt)
{
    if (m_pendingRebuild)
    {
        rebuildBatch();
    }

    //entities are sorted by font on addition
    auto& entities = getEntities();
    for (auto i = 0u; i < entities.size(); ++i)
    {
        auto& text = entities[i].getComponent<Text>();
        if (text.m_dirtyFlags & Text::Verts)
        {
            m_pendingRebuild = true;
            break;
        }
        if (text.m_dirtyFlags & Text::Colours)
        {
            //vert attribs changed so update sub buffer data
            //create data
            std::vector<float> vertexData;
            auto copyVertex = [&](uint32 idx)
            {
                vertexData.push_back(text.m_vertices[idx].position.x);
                vertexData.push_back(text.m_vertices[idx].position.y);
                vertexData.push_back(text.m_vertices[idx].position.z);
                vertexData.push_back(1.f);

                vertexData.push_back(text.m_vertices[idx].colour.r);
                vertexData.push_back(text.m_vertices[idx].colour.g);
                vertexData.push_back(text.m_vertices[idx].colour.b);
                vertexData.push_back(text.m_vertices[idx].colour.a);

                vertexData.push_back(text.m_vertices[idx].UV.x);
                vertexData.push_back(text.m_vertices[idx].UV.y);

                vertexData.push_back(static_cast<float>(i)); //for transform lookup
                vertexData.push_back(0.f);
            };
            for (auto j = 0u; j < text.m_vertices.size(); ++j)
            {
                copyVertex(j);
            }

            //find offset (and VBO id)
            auto vboIdx = (i > MaxTexts) ? i % MaxTexts : 0;

            //update sub data
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_buffers[vboIdx].first));
            glCheck(glBufferSubData(GL_ARRAY_BUFFER, text.m_vboOffset,
                vertexData.size() * sizeof(float), vertexData.data()));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            text.m_dirtyFlags &= ~Text::Colours;
        }

        auto tx = entities[i].getComponent<Transform>();
        ////if depth sorted set Z to -Y
        //if (m_depthAxis == DepthAxis::Y)
        //{
        //    auto pos = tx.getPosition();
        //    pos.z = -(pos.y / 100.f); //reduce this else we surpass clip plane
        //    tx.setPosition(pos);
        //}
        //get current transforms
        std::size_t buffIdx = (i > MaxTexts) ? i % MaxTexts : 0;
        m_bufferTransforms[buffIdx][i - (buffIdx * MaxTexts)] = tx.getWorldTransform();
    }
}

void TextRenderer::render(Entity camera)
{
    glCheck(glEnable(GL_CULL_FACE));
    //glCheck(glEnable(GL_DEPTH_TEST));
    glCheck(glEnable(GL_BLEND));
    glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glCheck(glBlendEquation(GL_FUNC_ADD));

    const auto& camComponent = camera.getComponent<Camera>();
    applyViewport(camComponent.viewport);

    const auto& camTx = camera.getComponent<Transform>();
    auto viewMat = glm::inverse(camTx.getWorldTransform());

    //bind shader and attrib arrays - TODO dow this for both shader types
    glCheck(glUseProgram(m_shaders[Font::Bitmap].shader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_shaders[Font::Bitmap].projectionUniformIndex, 1, GL_FALSE, glm::value_ptr(camComponent.projection * viewMat)));
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glUniform1i(m_shaders[Font::Bitmap].textureUniformIndex, 0));

    //foreach vbo bind and draw
    std::size_t idx = 0;
    for (const auto& batch : m_buffers)
    {
        const auto& transforms = m_bufferTransforms[idx++]; //TODO this should be same index as current buffer
        glCheck(glUniformMatrix4fv(m_shaders[Font::Bitmap].xformUniformIndex, static_cast<GLsizei>(transforms.size()), GL_FALSE, glm::value_ptr(transforms[0])));

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, batch.first));

        //bind attrib pointers
        for (auto i = 0u; i < m_shaders[Font::Bitmap].attribMap.size(); ++i)
        {
            glCheck(glEnableVertexAttribArray(m_shaders[Font::Bitmap].attribMap[i].location));
            glCheck(glVertexAttribPointer(m_shaders[Font::Bitmap].attribMap[i].location, m_shaders[Font::Bitmap].attribMap[i].size, GL_FLOAT, GL_FALSE, vertexSize,
                reinterpret_cast<void*>(static_cast<intptr_t>(m_shaders[Font::Bitmap].attribMap[i].offset))));
        }

        for (const auto& batchData : batch.second)
        {
            //CRO_ASSERT(batchData.texture > -1, "Missing sprite texture!");
            glCheck(glBindTexture(GL_TEXTURE_2D, batchData.texture));
            glCheck(glDrawArrays(GL_TRIANGLE_STRIP, batchData.start, batchData.count));
        }

        //unbind attrib pointers
        for (auto i = 0u; i < m_shaders[Font::Bitmap].attribMap.size(); ++i)
        {
            glCheck(glDisableVertexAttribArray(m_shaders[Font::Bitmap].attribMap[i].location));
        }

    }
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));


    //glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDisable(GL_BLEND));
    restorePreviousViewport();
}

//private
void TextRenderer::fetchShaderData(ShaderData& data)
{
    //check shader uniforms and get locations
    const auto& uniforms = data.shader.getUniformMap();
    auto listUniforms = [&uniforms]()
    {
        for (const auto& p : uniforms)
        {
            Logger::log(p.first, Logger::Type::Info);
        }
    };
    if (uniforms.count("u_worldMatrix[0]") != 0)
    {
        data.xformUniformIndex = uniforms.find("u_worldMatrix[0]")->second;
    }
    else
    {
        Logger::log("u_worldMatrix uniform missing from sprite renderer. Available uniforms are: ", Logger::Type::Info);
        listUniforms();
    }

    if (uniforms.count("u_texture") != 0)
    {
        data.textureUniformIndex = uniforms.find("u_texture")->second;
    }
    else
    {
        Logger::log("u_texture uniform missing from sprite renderer. Available uniforms are: ", Logger::Type::Info);
        listUniforms();
    }

    if (uniforms.count("u_projectionMatrix") != 0)
    {
        data.projectionUniformIndex = uniforms.find("u_projectionMatrix")->second;
    }
    else
    {
        Logger::log("u_projectionMatrix uniform missing from sprite renderer. Available uniforms are: ", Logger::Type::Info);
        listUniforms();
    }

    //map attrib locations
    const auto& attribMap = data.shader.getAttribMap();
    data.attribMap[AttribLocation::Position].size = 4;
    data.attribMap[AttribLocation::Position].location = attribMap[Mesh::Position];
    data.attribMap[AttribLocation::Colour].size = 4;
    data.attribMap[AttribLocation::Colour].location = attribMap[Mesh::Colour];
    data.attribMap[AttribLocation::Colour].offset = data.attribMap[AttribLocation::Colour].size * sizeof(float);
    data.attribMap[AttribLocation::UV0].size = 2;
    data.attribMap[AttribLocation::UV0].location = attribMap[Mesh::UV0];
    data.attribMap[AttribLocation::UV0].offset = data.attribMap[AttribLocation::Colour].offset + (data.attribMap[AttribLocation::Colour].size * sizeof(float));
    data.attribMap[AttribLocation::UV1].size = 2;
    data.attribMap[AttribLocation::UV1].location = attribMap[Mesh::UV1];
    data.attribMap[AttribLocation::UV1].offset = data.attribMap[AttribLocation::UV0].offset + (data.attribMap[AttribLocation::UV0].size * sizeof(float));

}

void TextRenderer::rebuildBatch()
{
    auto& entities = getEntities();
    auto vboCount = (entities.size() > MaxTexts) ? (entities.size() & MaxTexts) + 1 : 1;

    //allocate new VBO if needed
    auto neededVBOs = vboCount - m_buffers.size();
    for (auto i = 0u; i < neededVBOs; ++i)
    {
        uint32 vbo;
        glCheck(glGenBuffers(1, &vbo));
        m_buffers.emplace_back(std::make_pair(vbo, std::vector<Batch>()));
    }

    //create a batch for each VBO, sub indexing at MaxTexts
    uint32 start = 0;
    uint32 batchIdx = 0;
    for (auto& batch : m_buffers)
    {
        Batch batchData;
        batchData.start = start;
        batchData.texture = entities[batchIdx].getComponent<Text>().m_font->getTexture().getGLHandle();
        int32 spritesThisBatch = 0;

        std::vector<float> vertexData;
        auto maxCount = std::min(static_cast<uint32>(entities.size()), batchIdx + MaxTexts);
        for (auto i = 0u; i < maxCount; ++i)
        {
            auto& text = entities[i + batchIdx].getComponent<Text>();
            auto texID = text.m_font->getTexture().getGLHandle();
            if (texID != batchData.texture)
            {
                //end the batch and start a new one for this buffer
                batchData.count = start - batchData.start;
                batch.second.push_back(batchData);

                batchData.start = start;
                batchData.texture = texID;

                spritesThisBatch = 0;
            }
            updateVerts(text);

            //copies vertex data at a given index
            auto copyVertex = [&](uint32 idx)
            {
                vertexData.push_back(text.m_vertices[idx].position.x);
                vertexData.push_back(text.m_vertices[idx].position.y);
                vertexData.push_back(text.m_vertices[idx].position.z);
                vertexData.push_back(1.f);

                vertexData.push_back(text.m_vertices[idx].colour.r);
                vertexData.push_back(text.m_vertices[idx].colour.g);
                vertexData.push_back(text.m_vertices[idx].colour.b);
                vertexData.push_back(text.m_vertices[idx].colour.a);

                vertexData.push_back(text.m_vertices[idx].UV.x);
                vertexData.push_back(text.m_vertices[idx].UV.y);

                vertexData.push_back(static_cast<float>(i)); //for transform lookup
                vertexData.push_back(0.f); //not used right now
            };

            if (spritesThisBatch > 0)
            {
                //add degenerate tri
                start += 2;

                //copy last vert
                auto elementCount = vertexSize / sizeof(float);
                for (auto j = 0u; j < elementCount; ++j)
                {
                    vertexData.push_back(vertexData[vertexData.size() - elementCount]);
                }

                //copy impending vert
                copyVertex(0);
            }

            //increase the start point
            start += static_cast<uint32>(text.m_vertices.size());

            //and append data
            text.m_vboOffset = static_cast<int32>(vertexData.size() * sizeof(float));
            for (auto j = 0u; j < text.m_vertices.size(); ++j)
            {
                copyVertex(j);
            }
            spritesThisBatch++;

            text.m_dirtyFlags = 0;
        }

        batchIdx += MaxTexts;
        batchData.count = start - batchData.start;
        batch.second.push_back(batchData);

        //upload to VBO
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, batch.first));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW));
    }


    //allocate the space for transforms
    m_bufferTransforms.clear();
    m_bufferTransforms.resize(m_buffers.size());
    std::size_t i = 0;
    for (const auto& buffer : m_buffers)
    {
        m_bufferTransforms[i].resize((buffer.second.back().start + buffer.second.back().count) / 4);
        i++;
    }

    m_pendingRebuild = false;
}

void TextRenderer::updateVerts(Text& text)
{
    /*
    0-------2
    |       |
    |       |
    1-------3
    */
    CRO_ASSERT(text.m_font, "Must construct text with a font!");
    if (text.m_string.empty()) return;

    text.m_vertices.clear();
    text.m_vertices.reserve(text.m_string.size() * 6); //4 verts per char + degen tri

    float xPos = 0.f; //current char offset
    float yPos = -text.getLineHeight();
    float lineHeight = text.getLineHeight();
    glm::vec2 texSize(text.m_font->getTexture().getSize());
    CRO_ASSERT(texSize.x > 0 && texSize.y > 0, "Font texture not loaded!");

    float top = 0.f;
    float width = 0.f;

    for (auto c : text.m_string)
    {
        //check for end of lines
        if (c == '\r' || c == '\n')
        {
            xPos = 0.f;
            yPos -= lineHeight;
            continue;
        }

        auto rect = text.m_font->getGlyph(c);
        Text::Vertex v;
        v.position.x = xPos;
        v.position.y = yPos + rect.height;
        v.position.z = 0.f;

        v.UV.x = rect.left / texSize.x;
        v.UV.y = (rect.bottom + rect.height) / texSize.y;
        text.m_vertices.push_back(v);
        text.m_vertices.push_back(v); //twice for degen tri

        v.position.y = yPos;

        v.UV.x = rect.left / texSize.x;
        v.UV.y = rect.bottom / texSize.y;
        text.m_vertices.push_back(v);

        v.position.x = xPos + rect.width;
        v.position.y = yPos + rect.height;

        v.UV.x = (rect.left + rect.width) / texSize.x;
        v.UV.y = (rect.bottom + rect.height) / texSize.y;
        text.m_vertices.push_back(v);

        v.position.y = yPos;

        v.UV.x = (rect.left + rect.width) / texSize.x;
        v.UV.y = rect.bottom / texSize.y;
        text.m_vertices.push_back(v);
        text.m_vertices.push_back(v); //end degen tri

        if (v.position.x > width) width = v.position.x;

        xPos += rect.width;
    }

    text.m_localBounds.bottom = yPos;
    text.m_localBounds.height = top - yPos;
    text.m_localBounds.width = width;

    text.m_vertices.erase(text.m_vertices.begin()); //remove front/back degens as these are added by renderer
    text.m_vertices.pop_back();

    for (auto& v : text.m_vertices)
    {
        v.colour = { text.m_colour.getRed(), text.m_colour.getGreen(), text.m_colour.getBlue(), text.m_colour.getAlpha() };
    }
    text.m_dirtyFlags = 0;
}

void TextRenderer::onEntityAdded(Entity entity)
{
    CRO_ASSERT(entity.getComponent<Text>().m_font, "Text must be constructed with a font");
    
    //sort by font
    auto& entities = getEntities();
    std::sort(entities.begin(), entities.end(),
        [](Entity& a, Entity& b)
    {
        return (a.getComponent<Text>().m_font->getTexture().getGLHandle() >
            b.getComponent<Text>().m_font->getTexture().getGLHandle());
        //TODO subsort by type/shader
    });

    m_pendingRebuild = true;
}

void TextRenderer::onEntityRemoved(Entity)
{
    m_pendingRebuild = true;
}
