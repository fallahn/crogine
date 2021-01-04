/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include "Gizmo.hpp"
#include "GLCheck.hpp"

#include "im3d.h"

#include <crogine/core/App.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>

namespace
{
#include "GizmoPointShader.inl"
#include "GizmoLineShader.inl"
#include "GizmoTriangleShader.inl"

    //TODO unhackify this - also needed for setting frustum culling
    const float* vpMatrix = nullptr;
}

Gizmo::Gizmo()
    : m_frameLocked (false),
    m_frameEnded    (true),
    m_vao           (0),
    m_vbo           (0),
    m_ubo           (0),
    m_uboBlocksize  (0)
{
    
}

Gizmo::~Gizmo()
{
    deleteBuffers();
}

//public
void Gizmo::init()
{
    glCheck(glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &m_uboBlocksize));
    CRO_ASSERT(m_uboBlocksize > 0, "UBO block size is invalid");

    const std::string defines("#define BLOCK_SIZE " + std::to_string(m_uboBlocksize) + "\n#define AntiAliasing 2.0\n");

    m_pointShader.loadFromString(PointShader::Vertex, PointShader::Fragment, defines);
    m_lineShader.loadFromString(LineShader::Vertex, LineShader::Fragment, defines);
    m_triangleShader.loadFromString(TriangleShader::Vertex, TriangleShader::Fragment, defines);

    CRO_ASSERT(m_pointShader.getGLHandle(), "");
    CRO_ASSERT(m_lineShader.getGLHandle(), "");
    CRO_ASSERT(m_triangleShader.getGLHandle(), "");

    GLuint blockIndex = 0;
    glCheck(blockIndex = glGetUniformBlockIndex(m_pointShader.getGLHandle(), "VertexDataBlock"));
    glCheck(glUniformBlockBinding(m_pointShader.getGLHandle(), blockIndex, 0));

    glCheck(blockIndex = glGetUniformBlockIndex(m_lineShader.getGLHandle(), "VertexDataBlock"));
    glCheck(glUniformBlockBinding(m_lineShader.getGLHandle(), blockIndex, 0));

    glCheck(blockIndex = glGetUniformBlockIndex(m_triangleShader.getGLHandle(), "VertexDataBlock"));
    glCheck(glUniformBlockBinding(m_triangleShader.getGLHandle(), blockIndex, 0));

    //the gizmo is drawn by supplying a single quad then using
    //a UBO to provide index data coupled with instanced rendering
    std::array verts =
    {
        glm::vec4(-1.f, -1.f, 0.f, 1.f),
        glm::vec4( 1.f, -1.f, 0.f, 1.f),
        glm::vec4(-1.f,  1.f, 0.f, 1.f),
        glm::vec4( 1.f,  1.f, 0.f, 1.f)
    };

    glCheck(glGenBuffers(1, &m_vbo));
    glCheck(glGenVertexArrays(1, &m_vao));
    glCheck(glBindVertexArray(m_vao));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec4), verts.data(), GL_STATIC_DRAW));
    
    //we're not checking the location in the attrib map because
    //we're assuming there's only one attribute in the shader.
    glCheck(glEnableVertexAttribArray(0));
    glCheck(glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0));
    glCheck(glBindVertexArray(0));

    glCheck(glGenBuffers(1, &m_ubo));
}

void Gizmo::newFrame(float dt, cro::Entity camEnt)
{
    CRO_ASSERT(camEnt.hasComponent<cro::Camera>(), "");

    if (m_frameLocked)
    {
        return;
    }

    const auto& cam = camEnt.getComponent<cro::Camera>();
    const auto& tx = camEnt.getComponent<cro::Transform>();

    auto getViewport = [&]()
    {
        glm::vec2 windowSize(cro::App::getWindow().getSize());

        m_viewport =
        {
            static_cast<std::int32_t>(cam.viewport.left * windowSize.x),
            static_cast<std::int32_t>(cam.viewport.bottom * windowSize.y),
            static_cast<std::int32_t>(cam.viewport.width * windowSize.x),
            static_cast<std::int32_t>(cam.viewport.height * windowSize.y)
        };

        return Im3d::Vec2(windowSize.x * cam.viewport.width, windowSize.y * cam.viewport.height);
    };

    Im3d::AppData& appData = Im3d::GetAppData();
    appData.m_deltaTime = dt;
    appData.m_viewportSize = getViewport();
    appData.m_viewOrigin = tx.getWorldPosition();
    appData.m_viewDirection = tx.getForwardVector();
    appData.m_worldUp = { 0.f, 1.f, 0.f };
    appData.m_projOrtho = false;

    //if this were ortho we'd use the projection height
    appData.m_projScaleY = std::tan(cam.getFOV() * 0.5f) * 2.f;
    vpMatrix = &cam.getActivePass().viewProjectionMatrix[0][0];

    //ray picking
    auto cursorPos = cro::Mouse::getPosition();
    cursorPos.x /= appData.m_viewportSize.x;
    cursorPos.y /= appData.m_viewportSize.y;
    cursorPos = cursorPos * 2.f - 1.f;
    cursorPos.y = -cursorPos.y;

    //TODO this will need to be updated for orthographic projections
    glm::vec3 rayDirection =
    {
        cursorPos.x / cam.getProjectionMatrix()[0][0],
        cursorPos.y / cam.getProjectionMatrix()[1][1],
        -1.f
    };
    appData.m_cursorRayDirection = glm::vec3(tx.getWorldTransform() * glm::vec4(rayDirection, 0.f));
    appData.m_cursorRayOrigin = appData.m_viewOrigin;

    //TODO set the viewproj matrix for frustum culling of gizmos

    appData.m_keyDown[Im3d::Mouse_Left] = cro::Mouse::isButtonPressed(cro::Mouse::Button::Left);

    //TODO keyboard shortcuts for gizmo type (TRS etc)


    Im3d::NewFrame();
    m_frameLocked = true;
    m_frameEnded = false;
}

void Gizmo::draw()
{
    if (!m_frameEnded)
    {
        Im3d::EndFrame();
        m_frameEnded = true;
    }

    int oldVP[4];
    glCheck(glGetIntegerv(GL_VIEWPORT, oldVP));

    glCheck(glViewport(m_viewport.left, m_viewport.bottom, m_viewport.width, m_viewport.height));
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glEnable(GL_BLEND));
    glCheck(glBlendEquation(GL_FUNC_ADD));
    glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    glCheck(glBindVertexArray(m_vao));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

    glCheck(glBindBuffer(GL_UNIFORM_BUFFER, m_ubo));

    const auto& appData = Im3d::GetAppData();
    for (auto i = 0u; i < Im3d::GetDrawListCount(); ++i)
    {
        const auto& drawList = Im3d::GetDrawLists()[i];
        //TODO test layer ID to see if we need to update the blend func

        std::int32_t primitiveType = 0;
        std::uint32_t shaderID = 0;
        std::int32_t vertCount = 0;

        switch (drawList.m_primType)
        {
        default: break;
        case Im3d::DrawPrimitive_Points:
            primitiveType = GL_TRIANGLE_STRIP;
            vertCount = 1;
            shaderID = m_pointShader.getGLHandle();
            glCheck(glDisable(GL_CULL_FACE));
            break;
        case Im3d::DrawPrimitive_Lines:
            primitiveType = GL_TRIANGLE_STRIP;
            vertCount = 2;
            shaderID = m_lineShader.getGLHandle();
            glCheck(glDisable(GL_CULL_FACE));
            break;
        case Im3d::DrawPrimitive_Triangles:
            primitiveType = GL_TRIANGLES;
            vertCount = 3;
            shaderID = m_triangleShader.getGLHandle();
            break;
        }


        glCheck(glUseProgram(shaderID));
        //TODO cache these IDs so we don't have to keep querying the map
        glCheck(glUniform2f(glGetUniformLocation(shaderID, "u_viewportSize"), appData.m_viewportSize.x, appData.m_viewportSize.y));
        glCheck(glUniformMatrix4fv(glGetUniformLocation(shaderID, "u_viewProjectionMatrix"), 1, false, vpMatrix));

        //split the data into UBO sized chunks
        const std::int32_t primitivesPerPass = m_uboBlocksize / (sizeof(Im3d::VertexData) * vertCount);
        std::int32_t remainingCount = drawList.m_vertexCount / vertCount;
        const auto* vertData = drawList.m_vertexData;
        while (remainingCount > 0)
        {
            std::int32_t passPrimitiveCount = remainingCount < primitivesPerPass ? remainingCount : primitivesPerPass;
            std::int32_t passVertCount = passPrimitiveCount * vertCount;

            glCheck(glBufferData(GL_UNIFORM_BUFFER, passVertCount * sizeof(Im3d::VertexData), vertData, GL_DYNAMIC_DRAW));

            glCheck(glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_ubo)); //TODO can this be moved outside the loop?
            glCheck(glDrawArraysInstanced(primitiveType, 0, primitiveType == GL_TRIANGLES ? 3 : 4, passPrimitiveCount));

            vertData += passVertCount;
            remainingCount -= passPrimitiveCount;
        }
    }


    //TODO send text rendering to ImGui


    glCheck(glBindBuffer(GL_UNIFORM_BUFFER, 0));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    glCheck(glBindVertexArray(0));

    glCheck(glDisable(GL_BLEND));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glViewport(oldVP[0], oldVP[1], oldVP[2], oldVP[3]));
}

void Gizmo::unlock()
{
    m_frameLocked = false;
}

void Gizmo::finalise()
{
    //forcefully destruct the shaders as we're at the app level
    //and the default dtor won't be called until after OpenGL is tidied up
    m_pointShader = {};
    m_lineShader = {};
    m_triangleShader = {};

    //and any buffers we were using
    deleteBuffers();
}

//private
void Gizmo::deleteBuffers()
{
    if (m_ubo)
    {
        glCheck(glDeleteBuffers(1, &m_ubo));
    }

    if (m_vao)
    {
        glCheck(glDeleteVertexArrays(1, &m_vao));
    }

    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }

    m_vbo = 0;
    m_vao = 0;
    m_ubo = 0;
}