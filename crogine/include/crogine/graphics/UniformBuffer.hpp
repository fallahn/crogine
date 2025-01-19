/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2025
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

#include <crogine/Config.hpp>
#include <crogine/detail/SDLResource.hpp>
#ifdef CRO_DEBUG_
#include <crogine/gui/GuiClient.hpp>
#endif

#include <typeindex>
#include <unordered_map>
#include <vector>

namespace cro
{
    class Shader;
    namespace Detail
    {
        /*!
        \brief Base class of UniformBuffer
        \see UniformBuffer
        */
        class CRO_EXPORT_API UniformBufferImpl : public cro::Detail::SDLResource
#ifdef CRO_DEBUG_
            , public cro::GuiClient
#endif
        {
        public:
            /*!
            \brief Adds a shader to the uniform buffer.
            If the shader is found not to contain a uniform block with the
            same name as the one passed on construction then the shader is ignored.
            Shaders are reference counted, so mutiple insertions of a shader
            mean that the shader is not fully removed until the ref count is fully
            decremented to zero
            \see removeShader()
            */
            void addShader(const Shader&);
            void addShader(std::uint32_t);

            /*!
            \brief Removes a reference to a shader from the UBO
            This should be used when a shader goes out of use to decrement the
            reference count.
            */
            void removeShader(const Shader&);
            void removeShader(std::uint32_t);

            /*!
            \brief Binds the UniformBuffer and associated shaders ready for drawing.
            */
            void bind();

            /*!
            \brief Unbinds the UBO from its binding point
            This is called automatically in most cases (including destruction)
            so is only here if you need to manually re-assign binding points.
            */
            void unbind();

            /*!
            \brief Returns the max number of binding points available
            */
            static std::int32_t getMaxBindings();

            /*!
            \brief Returns true if shaders have been added to this UBO, else false
            */
            bool hasShaders() const { return !m_shaders.empty(); }

        protected:
            /*!
            \brief Constructor
            \param blockName The name of the uniform block as it appears in
            the shader to be used with this buffer.
            \param blockSize The size of the uniform block, including required padding.
            */
            UniformBufferImpl(const std::string& blockName, std::size_t blockSize);
            virtual ~UniformBufferImpl();

            UniformBufferImpl(const UniformBufferImpl&) = delete;
            UniformBufferImpl(UniformBufferImpl&&) noexcept;

            const UniformBufferImpl& operator = (const UniformBufferImpl&) = delete;
            const UniformBufferImpl& operator = (UniformBufferImpl&&) noexcept;


            void setBindingPoint(std::type_index);
            void setData(const void* data);


        private:
            std::string m_blockName;
            std::size_t m_bufferSize;
            std::uint32_t m_ubo;
            std::uint32_t m_bindPoint;

            //how much to adjust the instance count by.
            //we track this so when an instance is moved
            //it won't decrement the instance count.
            std::int32_t m_instanceCountOffset; 

            std::vector<std::pair<std::uint32_t, std::uint32_t>> m_shaders;
            //counts the number of instances of a shader being used with this UBO
            //so we make sure to only ever bind one instance of it at a time.
            std::unordered_map<std::uint32_t, std::uint32_t> m_refCount;

            void reset();
        };
    }

    /*!
    \brief Utility class around a OpenGL's uniform buffer object.
    Not available on mobile.

    This class can be used to more efficiently group together shader
    uniforms which are common between many shaders, for example elapsed
    game time. 

    UniformBuffer is moveable but non-copyable.

    Template parameter should be a type defining the data block used
    with the buffer
    \see setData()
    */
    template <class T>
    class UniformBuffer final : public Detail::UniformBufferImpl
    {
    public:
        /*!
        \brief Constructor
        \param blockName The name of the uniform block as it appears in
        the shader to be used with this buffer.
        */
        explicit UniformBuffer(const std::string& blockName)
            : Detail::UniformBufferImpl(blockName, sizeof(T))
        {
            setBindingPoint(typeid(T));
        }

        /*!
        \brief Pass a block of data to be uploaded to the uniform buffer.
        This should be the same type used when constructing the buffer. It
        is up to the user to make sure that the block data matches the shader
        uniform:

        \begincode
        //CPU side:
        struct MyData
        {
            float time = 0.f;
            float scale = 1.f;
        }
        UniformBuffer<MyData> buffer("MyShaderData");
        MyData someData;
        someData.time = elapsedTime;
        buffer.setData(someData);

        //shader code
        layout (std140) uniform MyShaderData
        {
            float u_time;
            float u_scale;
        };

        \endcode
        */
        void setData(const T& data)
        {
            Detail::UniformBufferImpl::setData(static_cast<const void*>(&data));
        }

    private:

    };
}