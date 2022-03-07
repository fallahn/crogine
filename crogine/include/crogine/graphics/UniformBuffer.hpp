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

	/*!
	\brief Utility class around a OpenGL's uniform buffer object.
	Not available on mobile.

	This class can be used to more effeciently group together shader
	uniforms which are common between many shaders, for example elapsed
	game time. Only really useful when using custom shaders, rather than
	the built in material shaders.

	UniformBuffer is moveable but non-copyable.
	*/
	class CRO_EXPORT_API UniformBuffer : public cro::Detail::SDLResource
	{
	public:
		/*!
		\brief Constructor
		\param blockName The name of the uniform block as it appears in
		the shader to be used with this buffer.
		\param blockSize The size of the uniform block, including required padding.
		*/
		UniformBuffer(const std::string& blockName, std::size_t blockSize);
		~UniformBuffer();

		UniformBuffer(const UniformBuffer&) = delete;
		UniformBuffer(UniformBuffer&&) noexcept;

		const UniformBuffer& operator = (const UniformBuffer&) = delete;
		const UniformBuffer& operator = (UniformBuffer&&) noexcept;

		/*!
		\brief Sets the data stored in the buffer. Expected to point
		to a data block with the same size in bytes with which the buffer
		was constructed. Larger buffers will not be fully copied, smaller
		buffers will create all kinds of problems. You have been warned.
		*/
		void setData(void* data);

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

	private:
		std::string m_blockName;
		std::size_t m_bufferSize;
		std::uint32_t m_ubo;

		std::vector<std::pair<std::uint32_t, std::uint32_t>> m_shaders;

		void reset();
	};
}