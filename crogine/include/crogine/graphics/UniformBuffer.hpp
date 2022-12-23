/*-----------------------------------------------------------------------

Matt Marchant 2022
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
		{
		public:
			/*!
			\brief Adds a shader to the uniform buffer.
			If the shader is found not to contain a uniform block with the
			same name as the one passed on construction then the shader is ignored.
			*/
			void addShader(const Shader&);

			/*!
			\brief Binds the UniformBuffer and associated shaders ready for drawing.
			This will bind the UBO at the specified bind point if it is valid,
			which will also unbind any UniformBuffer that may already be bound to
			that point.
			*/
			void bind(std::uint32_t bindPoint);

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


			void setData(const void* data);


		private:
			std::string m_blockName;
			std::size_t m_bufferSize;
			std::uint32_t m_ubo;

			std::vector<std::pair<std::uint32_t, std::uint32_t>> m_shaders;

			void reset();
		};
	}

	/*!
	\brief Utility class around a OpenGL's uniform buffer object.
	Not available on mobile.

	This class can be used to more efficiently group together shader
	uniforms which are common between many shaders, for example elapsed
	game time. Only really useful when using custom shaders, rather than
	the built in material shaders (which have no uniform blocks defined).

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