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

#include "../detail/GLCheck.hpp"
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/graphics/Shader.hpp>

using namespace cro;

UniformBuffer::UniformBuffer(const std::string& blockName, std::size_t dataSize)
	: m_blockName(blockName),
	m_bufferSize(dataSize),
	m_ubo(0)
{
#ifdef PLATFORM_DESKTOP
	CRO_ASSERT(!blockName.empty(), "");
	CRO_ASSERT(dataSize, "");

	//TODO create buffer
#else
	CRO_ASSERT(true, "Not available on mobile");
#endif

}

UniformBuffer::~UniformBuffer()
{
#ifdef PLATFORM_DESKTOP
	if (m_ubo)
	{

	}
#endif
}

UniformBuffer::UniformBuffer(UniformBuffer&& other) noexcept
{

}

const UniformBuffer& UniformBuffer::operator = (UniformBuffer&& other) noexcept
{

	return *this;
}

//public
void UniformBuffer::setData(void* data, std::size_t size)
{
#ifdef PLATFORM_DESKTOP
	CRO_ASSERT(data, "");
	CRO_ASSERT(size, "");
	CRO_ASSERT(m_ubo, "");

#endif // PLATFORM_DESKTOP
}

void UniformBuffer::addShader(const Shader& shader)
{
#ifdef PLATFORM_DESKTOP
	CRO_ASSERT(shader.getGLHandle(), "");

	//if shader has uniform block matching our name, add

#endif
}

void UniformBuffer::bind(std::uint32_t bindPoint)
{
#ifdef PLATFORM_DESKTOP
	CRO_ASSERT(bindPoint < GL_MAX_UNIFORM_BUFFER_BINDINGS, "");

	//bind ubo to bind point

	for (auto [shader, blockID] : m_shaders)
	{
		//bind to bind point
	}

#endif
}