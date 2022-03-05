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
	class CRO_EXPORT_API UniformBuffer : public cro::Detail::SDLResource
	{
	public:
		UniformBuffer(const std::string& blockName, std::size_t blockSize);
		~UniformBuffer();

		UniformBuffer(const UniformBuffer&) = delete;
		UniformBuffer(UniformBuffer&&) noexcept;

		const UniformBuffer& operator = (const UniformBuffer&) = delete;
		const UniformBuffer& operator = (UniformBuffer&&) noexcept;

		void setData(void* data, std::size_t size);

		void addShader(const Shader&);

		void bind(std::uint32_t bindPoint);

	private:
		std::string m_blockName;
		std::size_t m_bufferSize;
		std::uint32_t m_ubo;

		std::vector<std::pair<std::uint32_t, std::uint32_t>> m_shaders;
	};
}