/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef CRO_SPRITE_RENDERER_HPP_
#define CRO_SPRITE_RENDERER_HPP_

#include <crogine/Config.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/detail/SDLResource.hpp>

#include <glm/mat4x4.hpp>

#include <map>

namespace cro
{
    /*!
    \brief Batches and renders the scene Sprite components
    */
    class CRO_EXPORT_API SpriteRenderer final : public System, public Detail::SDLResource
    {
    public:
        SpriteRenderer();
        ~SpriteRenderer();

        //TODO setters for view/resolution

        /*!
        \brief Implements the process which performs batching
        */
        void process(Time) override;

        /*!
        \brief Renders the Sprite components
        */
        void render();

    private:
        //maps VBO to texture
        std::map<uint32, uint32> m_buffers;

        glm::mat4 m_projectionMatrix;
        Shader m_shader;
    };
}

#endif //CRO_SPRITE_RENDERER_HPP_