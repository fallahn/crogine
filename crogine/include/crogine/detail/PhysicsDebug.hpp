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

#pragma once

#include <crogine/graphics/Shader.hpp>

#include <LinearMath/btIDebugDraw.h>

#include <crogine/detail/glm/mat4x4.hpp>

#include <vector>

namespace cro
{
    namespace Detail
    {
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

            void drawLine(const btVector3&, const btVector3&, const btVector3&) override;

            void drawContactPoint(const btVector3&, const btVector3&, btScalar, int, const btVector3&) override {}
            void reportErrorWarning(const char*) override;
            void draw3dText(const btVector3&, const char*) override {}
            void setDebugMode(int) override;
            int getDebugMode() const override;

            void render(glm::mat4);

        private:
            uint32 m_vboID;
            std::array<int32, 2u> m_attribIndices{};
            std::vector<float> m_vertexData;
            std::size_t m_vertexCount;
            Shader m_shader;
            int32 m_uniformIndex;
            int32 m_debugFlags;
        };
    }
}