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

#ifndef CRO_POST_PROCESS_HPP_
#define CRO_POST_PROCESS_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/core/Clock.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <map>
#include <string>

namespace cro
{
    class RenderTexture;
    class Shader;
    class Texture;

    /*!
    \brief Post Process interface.
    Post processes take a reference to an input buffer to which they
    apply a given effect, before drawing it to the currently active buffer.
    A post process effect can be added to a scene and is applied to the entire
    output, so to prevent a particular process affecting the UI for example then
    a separate scene should be created for 2D elements.

    When creating custom shaders for a post process it is recommended to use the
    vertex shader (or at least copy the attribute layout) in graphics/shaders/PostVertex.hpp

    */
    class CRO_EXPORT_API PostProcess : public Detail::SDLResource
    {
    public:
        PostProcess();
        virtual ~PostProcess();
        PostProcess(const PostProcess&) = delete;
        PostProcess(PostProcess&&) = delete;
        const PostProcess& operator = (const PostProcess&) = delete;
        PostProcess& operator = (PostProcess&&) = delete;

        /*!
        \brief Optionally implement this if the effect requires updating
        over time, for example animated uniform values.
        */
        virtual void process(Time) {};

        /*!
        \brief Applies the Post Process to the source texture and
        renders it to the current active buffer.
        Called automatically by the scene to which this effect belongs
        */
        virtual void apply(const RenderTexture& source) = 0;

        /*
        \brief Used by crogine to update the post process should the buffer be resized.
        This should not be called by the user.
        */
        void resizeBuffer(int32 w, int32 h);

    protected:
        /*!
        \brief Draws a quad to the current buffer using the given shader
        \param shader Shader to use
        \param size Size of the quad to draw
        */
        void drawQuad(Shader& shader, FloatRect size);

        /*!
        \brief Sets the value of a uniform in the given shader, if it exists
        */
        void setUniform(const std::string& name, float value, const Shader& shader);
        void setUniform(const std::string& name, glm::vec2 value, const Shader& shader);
        void setUniform(const std::string& name, glm::vec3 value, const Shader& shader);
        void setUniform(const std::string& name, glm::vec4 value, const Shader& shader);
        void setUniform(const std::string& name, Colour value, const Shader& shader);
        void setUniform(const std::string& name, const Texture& value, const Shader& shader);

        /*!
        \brief Called when main output buffer resized.
        Override this is you need to resize any intermediate render textures. Use
        getCurrentBufferSize() to read the new value. This is gaurenteed to be called at
        least once for each post process so it is safe to initialise any intermediate
        buffers here, rather than upon construction.
        */
        virtual void bufferResized() {}

        /*!
        \brief Returns the current output buffer size (normally the window resolution)
        */
        glm::uvec2 getCurrentBufferSize() const { return m_currentBufferSize; }

    private:
        glm::uvec2 m_currentBufferSize;
        
        uint32 m_vbo;
        glm::mat4 m_projection;
        glm::mat4 m_transform;

        struct UniformData final
        {
            enum
            {
                Number,
                Number2,
                Number3,
                Number4,
                Texture,
                None
            } type = None;

            union
            {
                float numberValue[4];
                uint32 textureID;
            };
        };

        std::map<uint32, std::map<uint32, UniformData>> m_uniforms;
    };
}

#endif //CRO_POST_PROCESS_HPP_