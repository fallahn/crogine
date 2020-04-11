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

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>

using namespace cro;

namespace
{
    uint32 MaxTexts = 127u; //this can be as low as 63 on mobile or high as 511
    constexpr uint32 vertexSize = (4 + 4 + 2 + 2) * sizeof(float); //pos, colour, UV0, UV1
}

TextRenderer::TextRenderer(MessageBus& mb)
    : System                (mb, typeid(TextRenderer)),
    m_pendingRebuild        (false),
    m_pendingSorting        (false)
{
    GLint maxVec;
    glCheck(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVec));
    MaxTexts = maxVec / 4; //4 x 4-components make up a mat4.
    MaxTexts = std::min(MaxTexts - 1, 255u); //caps size on platforms such as VMs which incorrectly report max_vectors
    LOG(std::to_string(MaxTexts) + " texts are available per batch", Logger::Type::Info);

    if (!m_shaders[/*Font::Type::Bitmap*/0].shader.loadFromString(Shaders::Sprite::Vertex, Shaders::Text::BitmapFragment, "#define MAX_MATRICES " + std::to_string(MaxTexts) + "\n"))
    {
        Logger::log("Failed loading bitmap font shader, text renderer is in invalid state", Logger::Type::Error, Logger::Output::All);
    }

    //if (!m_shaders[Font::Type::SDF].shader.loadFromString(Shaders::Sprite::Vertex, Shaders::Text::SDFFragment, "#define MAX_MATRICES " + std::to_string(MaxTexts) + "\n"))
    //{
    //    Logger::log("Failed loading SDF font shader, text renderer is in invalid state", Logger::Type::Error, Logger::Output::All);
    //}
    fetchShaderData(m_shaders[/*Font::Type::Bitmap*/0]);
    //fetchShaderData(m_shaders[Font::SDF]);

    requireComponent<Text>();
    requireComponent<Transform>();
}

TextRenderer::~TextRenderer()
{
    for (auto b : m_buffers)
    {
        glCheck(glDeleteBuffers(1, &b.first.vbo));

#ifdef PLATFORM_DESKTOP
        if (b.first.vao)
        {
            glCheck(glDeleteVertexArrays(1, &b.first.vao));
        }
#endif //PLATFORM
    }
}

//public
void TextRenderer::handleMessage(const Message& msg)
{

}

void TextRenderer::process(float)
{
    if (m_pendingRebuild)
    {
        rebuildBatch();
    }

    auto& entities = getEntities();
    for (auto i = 0u; i < entities.size(); ++i)
    {
        auto& text = entities[i].getComponent<Text>();
        if (text.m_dirtyFlags & (Text::Verts | Text::CharSize | Text::BlendMode))
        {
            m_pendingRebuild = true;
            m_pendingSorting = true;
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
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_buffers[vboIdx].first.vbo));
            glCheck(glBufferSubData(GL_ARRAY_BUFFER, text.m_vboOffset,
                vertexData.size() * sizeof(float), vertexData.data()));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            text.m_dirtyFlags &= ~Text::Colours;
        }

        const auto& tx = entities[i].getComponent<Transform>();

        //get current transforms
        std::size_t buffIdx = (i > MaxTexts) ? i % MaxTexts : 0;
        std::size_t batchIdx = i - (buffIdx * MaxTexts);
        auto worldTx = tx.getWorldTransform();
        m_bufferTransforms[buffIdx][batchIdx] = worldTx;
        m_buffers[text.m_batchIndex[0]].second[text.m_batchIndex[1]].worldScissor = text.m_croppingArea.transform(worldTx);
    }

    if (m_pendingSorting)
    {
        m_pendingSorting = false;

        //sort by font
        std::sort(entities.begin(), entities.end(),
            [](const Entity& a, const Entity& b)
        {
            const auto& textA = a.getComponent<Text>();
            const auto& textB = b.getComponent<Text>();

            if (textA.m_font->getTexture(textA.m_charSize).getGLHandle() <
                textB.m_font->getTexture(textB.m_charSize).getGLHandle()) return true;

            if (textB.m_font->getTexture(textB.m_charSize).getGLHandle() <
                textA.m_font->getTexture(textA.m_charSize).getGLHandle()) return false;

            if (textA.m_blendMode < textB.m_blendMode) return true;

            return false;
        });

        m_pendingRebuild = true;
    }
}

void TextRenderer::render(Entity camera)
{
    const auto& camComponent = camera.getComponent<Camera>();
    m_currentViewport = applyViewport(camComponent.viewport);

    //bind shader and attrib arrays - TODO do this for both shader types
    glCheck(glUseProgram(m_shaders[/*Font::Bitmap*/0].shader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_shaders[/*Font::Bitmap*/0].projectionUniformIndex, 1, GL_FALSE, &camComponent.viewProjectionMatrix[0][0]));
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glUniform1i(m_shaders[/*Font::Bitmap*/0].textureUniformIndex, 0));

    //foreach vbo bind and draw
    std::size_t idx = 0;
    for (const auto& [batchMap, batch] : m_buffers)
    {
        if (batch.empty())
        {
            continue;
        }

        const auto& transforms = m_bufferTransforms[idx++];
        glCheck(glUniformMatrix4fv(m_shaders[/*Font::Bitmap*/0].xformUniformIndex, static_cast<GLsizei>(transforms.size()), GL_FALSE, glm::value_ptr(transforms[0])));


#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(batchMap.vao));

        for (const auto& batchData : batch)
        {
            applyBlendMode(batchData.blendMode);

            if (batchData.scissor)
            {
                applyScissor(batchData.worldScissor, camComponent.viewProjectionMatrix);
            }

            glCheck(glBindTexture(GL_TEXTURE_2D, batchData.texture));
            glCheck(glDrawArrays(GL_TRIANGLE_STRIP, batchData.start, batchData.count));
            glCheck(glDisable(GL_SCISSOR_TEST));
        }
        glCheck(glBindVertexArray(0));
#else
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, batchMap.vbo));

        //bind attrib pointers
        for (auto i = 0u; i < m_shaders[/*Font::Bitmap*/0].attribMap.size(); ++i)
        {
            glCheck(glEnableVertexAttribArray(m_shaders[/*Font::Bitmap*/0].attribMap[i].location));
            glCheck(glVertexAttribPointer(m_shaders[/*Font::Bitmap*/0].attribMap[i].location, m_shaders[/*Font::Bitmap*/0].attribMap[i].size, GL_FLOAT, GL_FALSE, vertexSize,
                reinterpret_cast<void*>(static_cast<intptr_t>(m_shaders[/*Font::Bitmap*/0].attribMap[i].offset))));
        }

        for (const auto& batchData : batch)
        {
            //CRO_ASSERT(batchData.texture > -1, "Missing sprite texture!");
            applyBlendMode(batchData.blendMode);

            if (batchData.scissor)
            {
                applyScissor(batchData.worldScissor, camComponent.viewProjectionMatrix);
            }

            glCheck(glBindTexture(GL_TEXTURE_2D, batchData.texture));
            glCheck(glDrawArrays(GL_TRIANGLE_STRIP, batchData.start, batchData.count));
            glCheck(glDisable(GL_SCISSOR_TEST));
        }

        //unbind attrib pointers
        for (auto i = 0u; i < m_shaders[/*Font::Bitmap*/0].attribMap.size(); ++i)
        {
            glCheck(glDisableVertexAttribArray(m_shaders[/*Font::Bitmap*/0].attribMap[i].location));
        }
#endif //PLATFORM
    }
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));


    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDisable(GL_BLEND));
    glCheck(glDepthMask(GL_TRUE));
    
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
    auto vboCount = (entities.size() > MaxTexts) ? (entities.size() / MaxTexts) + 1 : 1;

    //allocate new VBO if needed
    auto neededVBOs = vboCount - m_buffers.size();
    for (auto i = 0u; i < neededVBOs; ++i)
    {
        BatchMap bm;
        glCheck(glGenBuffers(1, &bm.vbo));

        //use VAO on desktop builds
#ifdef PLATFORM_DESKTOP
        glCheck(glGenVertexArrays(1, &bm.vao));
        glCheck(glBindVertexArray(bm.vao));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, bm.vbo));

        for (auto j = 0u; j < m_shaders[/*Font::Bitmap*/0].attribMap.size(); ++j)
        {
            glCheck(glEnableVertexAttribArray(m_shaders[/*Font::Bitmap*/0].attribMap[j].location));
            glCheck(glVertexAttribPointer(m_shaders[/*Font::Bitmap*/0].attribMap[j].location, m_shaders[/*Font::Bitmap*/0].attribMap[j].size, GL_FLOAT, GL_FALSE, vertexSize,
                reinterpret_cast<void*>(static_cast<intptr_t>(m_shaders[/*Font::Bitmap*/0].attribMap[j].offset))));
        }
        //glCheck(glEnableVertexAttribArray(0));
        glCheck(glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif //PLATFORM

        m_buffers.emplace_back(std::make_pair(bm, std::vector<Batch>()));
    }

    //create a batch for each VBO, sub indexing at MaxTexts
    uint32 start = 0;
    uint32 batchIdx = 0;
    for (auto& [batchMap, batch] : m_buffers)
    {
        batch.clear();

        if (entities.empty())
        {
            continue;
        }

        auto& currText = entities[batchIdx].getComponent<Text>();
        
        Batch batchData;
        batchData.start = start;
        batchData.texture = currText.m_font->getTexture(currText.getCharSize()).getGLHandle();
        batchData.blendMode = currText.m_blendMode;
        batchData.scissor = currText.m_scissor;
        //scissor area is updated during processing, so we store the batch ID in the text component
        currText.m_batchIndex[0] = batchIdx;
        currText.m_batchIndex[1] = batch.size();

        int32 spritesThisBatch = 0;

        std::vector<float> vertexData;
        auto maxCount = std::min(static_cast<uint32>(entities.size()), batchIdx + MaxTexts);
        for (auto i = 0u; i < maxCount; ++i)
        {
            auto& text = entities[i + batchIdx].getComponent<Text>();
            text.updateVerts();
            text.m_dirtyFlags = 0;

            if (text.m_vertices.empty())
            {
                continue;
            }

            auto texID = text.m_font->getTexture(text.m_charSize).getGLHandle();
            //new batches are created within the VBO for each new text, blend mode or scissor mode
            if (texID != batchData.texture
                || text.m_blendMode != batchData.blendMode
                || text.m_scissor //every scissor usually has its own batch, as the cropping area will be different
                || text.m_scissor != batchData.scissor) 
            {
                //end the batch and start a new one for this buffer
                batchData.count = start - batchData.start;
                batch.push_back(batchData);

                batchData.start = start;
                batchData.texture = texID;
                batchData.blendMode = text.m_blendMode;
                batchData.scissor = text.m_scissor;
                batchData.worldScissor = text.m_croppingArea;

                spritesThisBatch = 0;
            }

            text.m_batchIndex[0] = batchIdx;
            text.m_batchIndex[1] = batch.size();

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
        batch.push_back(batchData);

        //upload to VBO
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, batchMap.vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW));
    }


    //allocate the space for transforms
    m_bufferTransforms.clear();
    m_bufferTransforms.resize(m_buffers.size());
    std::size_t i = 0;
    for (const auto& [batchMap, batch] : m_buffers)
    {
        if (!batch.empty())
        {
            m_bufferTransforms[i].resize((batch.back().start + batch.back().count) / 4);
        }
        i++;
    }

    m_pendingRebuild = false;
}

void TextRenderer::applyBlendMode(Material::BlendMode mode)
{
    switch (mode)
    {
    default: break;
    case Material::BlendMode::Additive:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glBlendFunc(GL_ONE, GL_ONE));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Alpha:
        glCheck(glEnable(GL_CULL_FACE));
        //glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Multiply:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::None:
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glDisable(GL_BLEND));
        break;
    }
}

void TextRenderer::applyScissor(const FloatRect& worldBox, const glm::mat4& viewProj)
{
    glm::vec4 pos = viewProj * glm::vec4(worldBox.left, worldBox.bottom, 0.f, 1.f);
    pos.x /= pos.w;
    pos.y /= pos.w;
    //pos.z /= pos.w;

    GLint x = static_cast<GLint>(((pos.x + 1.f) / 2.f)) *  m_currentViewport.width + m_currentViewport.left;
    GLint y = (static_cast<GLint>((pos.y + 1.f) / 2.f) *  m_currentViewport.height) + m_currentViewport.bottom;

    glm::vec4 size = viewProj * glm::vec4(worldBox.width + worldBox.left, worldBox.height + worldBox.bottom, 0.f, 1.f);
    size.x /= size.w;
    size.y /= size.w;

    GLint w = static_cast<GLint>((size.x + 1.f) / 2.f) * m_currentViewport.width + m_currentViewport.left;
    w -= x;
    GLint h = (static_cast<GLint>((size.y + 1.f) / 2.f) * m_currentViewport.height) + m_currentViewport.bottom;
    h -= y;

    //DPRINT("Scissor Pre", std::to_string(worldBox.left) + ", " + std::to_string(worldBox.bottom) + ", " + std::to_string(worldBox.width) + ", " + std::to_string(worldBox.height));
    //DPRINT("Scissor Post", std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(w) + ", " + std::to_string(h));

    glCheck(glScissor(x, y, w, h));
    glCheck(glEnable(GL_SCISSOR_TEST));
    //LOG("Scissor Applied", Logger::Type::Info);
}

void TextRenderer::onEntityAdded(Entity entity)
{
    CRO_ASSERT(entity.getComponent<Text>().m_font, "Text must be constructed with a font");
    
    m_pendingRebuild = true;
    m_pendingSorting = true;
}

void TextRenderer::onEntityRemoved(Entity)
{
    m_pendingRebuild = true;
}
