/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/graphics/Shader.hpp>
#include <crogine/detail/glm/mat4x4.hpp>

#include <LinearMath/btIDebugDraw.h>

#include <vector>

/*!
\brief Implements the bullet debug drawer
*/
class BulletDebug final : public btIDebugDraw
{
public:
    BulletDebug();
    ~BulletDebug();

    BulletDebug(const BulletDebug&) = delete;
    BulletDebug(BulletDebug&&) = delete;

    const BulletDebug& operator = (const BulletDebug&) = delete;
    BulletDebug& operator = (BulletDebug&&) = delete;

    void drawLine(const btVector3& start, const btVector3& end, const btVector3& colour) override;

    void drawContactPoint(const btVector3& point, const btVector3& normal, btScalar distance, int lifetime, const btVector3& colour) override {}
    void reportErrorWarning(const char*) override;
    void draw3dText(const btVector3&, const char*) override;
    void setDebugMode(int) override;
    int getDebugMode() const override;

    void render(glm::mat4 viewProjection, glm::uvec2);

    static const std::int32_t DebugFlags = btIDebugDraw::DBG_DrawNormals | btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_FastWireframe;

private:
    
    struct MeshData final
    {
        std::uint32_t vbo = 0;
        std::uint32_t vao = 0;
    }m_meshData;

    struct Vertex final
    {
        glm::vec3 position = glm::vec3(0.f);
        glm::vec3 colour = glm::vec3(0.f);
    };
    std::vector<Vertex> m_vertexData;
    std::size_t m_vertexCount;

    cro::Shader m_shader;
    std::int32_t m_uniformIndex;
    std::int32_t m_debugFlags;

    bool m_clearVerts;
};