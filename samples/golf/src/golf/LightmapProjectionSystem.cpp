/*-----------------------------------------------------------------------

Matt Marchant 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "LightmapProjectionSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/graphics/RenderTexture.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    static inline constexpr glm::vec2 LightMapWorldSize(LightMapSize);
    static inline constexpr glm::vec2 LightMapPixelsPerMetre(LightMapWorldSize / glm::vec2(LightMapWorldCoords.width, LightMapWorldCoords.height));
}

LightmapProjectionSystem::LightmapProjectionSystem(cro::MessageBus& mb, cro::RenderTexture* rt)
    : cro::System(mb, typeid(LightmapProjectionSystem)),
    m_renderTexture(rt)
{
    CRO_ASSERT(rt, "");
    requireComponent<LightmapProjector>();
    requireComponent<cro::Transform>();

    m_texture.loadFromFile("assets/golf/images/light_spot.png");

    m_vertexArray.setTexture(m_texture);
    m_vertexArray.setPrimitiveType(GL_TRIANGLES);
    m_vertexArray.setBlendMode(cro::Material::BlendMode::Additive);
    
    /*m_vertexArray.setVertexData({
        cro::Vertex2D(glm::vec2(0.f, 120.f), glm::vec2(0.f, 1.f)),
        cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f)),
        cro::Vertex2D(glm::vec2(120.f), glm::vec2(1.f)),
        cro::Vertex2D(glm::vec2(120.f, 0.f), glm::vec2(1.f, 0.f))
        });
    m_vertexArray.setPosition({ 512.f, 512.f });*/

    //registerWindow([&]() 
    //    {
    //        ImGui::Begin("sdfsg");
    //        ImGui::Image(m_renderTexture->getTexture(), { 256.f,128.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //        for (auto e : getEntities())
    //        {
    //            auto pos = toVertPosition(e.getComponent<cro::Transform>().getWorldPosition());
    //            ImGui::Text("Position: %3.2f, %3.2f", pos.x, pos.y);
    //        }
    //        ImGui::End();
    //    });
}

//public
void LightmapProjectionSystem::process(float dt)
{
    static constexpr float AnimFrameTime = 0.1f;

    auto& entities = getEntities();
    if (!entities.empty())
    {
        std::vector<cro::Vertex2D> verts;

        for (auto entity : entities)
        {
            auto& projector = entity.getComponent<LightmapProjector>();
            float animValue = 1.f;
            if (!projector.pattern.empty())
            {
                animValue = projector.pattern[projector.currentIndex];

                projector.animationTime += dt;
                if (projector.animationTime > AnimFrameTime)
                {
                    projector.currentIndex = (projector.currentIndex + 1) % projector.pattern.size();
                    projector.animationTime -= AnimFrameTime;
                }
            }



            const auto pos = entity.getComponent<cro::Transform>().getWorldPosition();
            const glm::vec2 vertPos = toVertPosition(pos);
            auto vertSize = (glm::vec2(projector.size) * LightMapPixelsPerMetre) / 2.f;

            float edgeFade = glm::smoothstep(0.f, vertSize.x, vertPos.x) * (1.f - glm::smoothstep(LightMapWorldSize.x - vertSize.x, LightMapWorldSize.x, vertPos.x));
            edgeFade *= glm::smoothstep(0.f, vertSize.y, vertPos.y);

            vertSize *= edgeFade;
            const auto c = projector.colour.getVec4() * projector.brightness * animValue * edgeFade;

            verts.emplace_back(glm::vec2(-vertSize.x, vertSize.y) + vertPos, glm::vec2(0.f, 1.f), c);
            verts.emplace_back(-vertSize + vertPos, glm::vec2(0.f), c);
            verts.emplace_back(vertSize + vertPos, glm::vec2(1.f), c);

            verts.emplace_back(vertSize + vertPos, glm::vec2(1.f), c);
            verts.emplace_back(-vertSize + vertPos, glm::vec2(0.f), c);
            verts.emplace_back(glm::vec2(vertSize.x, -vertSize.t) + vertPos, glm::vec2(1.f, 0.f), c);
        }
        m_vertexArray.setVertexData(verts);

        m_renderTexture->clear();
        m_vertexArray.draw();
        m_renderTexture->display();
    }
}

//private
glm::vec2 LightmapProjectionSystem::toVertPosition(glm::vec3 pos) const
{
    //z is inverse
    pos.z *= -1.f;

    pos.x -= LightMapWorldCoords.left;
    pos.z -= LightMapWorldCoords.bottom; 

    return glm::vec2(pos.x, pos.z) * LightMapPixelsPerMetre;
}