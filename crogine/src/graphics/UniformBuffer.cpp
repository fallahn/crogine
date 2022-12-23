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

Detail::UniformBufferImpl::UniformBufferImpl(const std::string& blockName, std::size_t dataSize)
	: m_blockName	(blockName),
	m_bufferSize	(dataSize),
	m_ubo			(0)
{
#ifdef PLATFORM_DESKTOP
	CRO_ASSERT(!blockName.empty(), "");
	CRO_ASSERT(dataSize, "");

	//create buffer
	glCheck(glGenBuffers(1, &m_ubo));
	glCheck(glBindBuffer(GL_UNIFORM_BUFFER, m_ubo));
	glCheck(glBufferData(GL_UNIFORM_BUFFER, dataSize, NULL, GL_DYNAMIC_DRAW));
	glCheck(glBindBuffer(GL_UNIFORM_BUFFER, 0));

#else
	CRO_ASSERT(true, "Not available on mobile");
#endif

}

Detail::UniformBufferImpl::~UniformBufferImpl()
{
#ifdef PLATFORM_DESKTOP
	reset();
#endif
}

Detail::UniformBufferImpl::UniformBufferImpl(Detail::UniformBufferImpl&& other) noexcept
	: m_blockName	(other.m_blockName),
	m_bufferSize	(other.m_bufferSize),
	m_ubo			(other.m_ubo)
{
	m_shaders.swap(other.m_shaders);

	other.m_blockName.clear();
	other.m_bufferSize = 0;
	other.m_ubo = 0;

	other.m_shaders.clear();
}

const Detail::UniformBufferImpl& Detail::UniformBufferImpl::operator = (Detail::UniformBufferImpl&& other) noexcept
{
	if (&other != this)
	{
		reset();

		m_blockName = other.m_blockName;
		m_bufferSize = other.m_bufferSize;
		m_ubo = other.m_ubo;

		m_shaders.swap(other.m_shaders);

		other.m_blockName.clear();
		other.m_bufferSize = 0;
		other.m_ubo = 0;

		other.m_shaders.clear();
	}
	return *this;
}

//public
void Detail::UniformBufferImpl::addShader(const Shader& shader)
{
#ifdef PLATFORM_DESKTOP
	CRO_ASSERT(shader.getGLHandle(), "");

	//if shader has uniform block matching our name, add
	auto blockIndex = glGetUniformBlockIndex(shader.getGLHandle(), m_blockName.c_str());
	if (blockIndex != GL_INVALID_INDEX)
	{
		m_shaders.emplace_back(shader.getGLHandle(), blockIndex);
	}
#ifdef CRO_DEBUG_
	else
	{
		LogW << "Shader " << shader.getGLHandle() << " was not added to " << m_blockName << std::endl;
	}
#endif
#endif
}

void Detail::UniformBufferImpl::bind(std::uint32_t bindPoint)
{
#ifdef PLATFORM_DESKTOP
#ifdef CRO_DEBUG_
	std::int32_t maxBindings = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxBindings);
	CRO_ASSERT(bindPoint < maxBindings, "");
#endif

	//bind ubo to bind point
	glCheck(glBindBufferBase(GL_UNIFORM_BUFFER, bindPoint, m_ubo));

	for (auto [shader, blockID] : m_shaders)
	{
		//bind to bind point
		glCheck(glUniformBlockBinding(shader, blockID, bindPoint));
	}
#endif
}

//protected
void Detail::UniformBufferImpl::setData(const void* data)
{
#ifdef PLATFORM_DESKTOP
	CRO_ASSERT(data, "");
	CRO_ASSERT(m_bufferSize, "");
	CRO_ASSERT(m_ubo, "");

	glCheck(glBindBuffer(GL_UNIFORM_BUFFER, m_ubo));
	glCheck(glBufferData(GL_UNIFORM_BUFFER, m_bufferSize, data, GL_DYNAMIC_DRAW));

#endif // PLATFORM_DESKTOP
}

//private
void Detail::UniformBufferImpl::reset()
{
#ifdef PLATFORM_DESKTOP
	if (m_ubo)
	{
		glCheck(glDeleteBuffers(1, &m_ubo));
		m_ubo = 0;
	}
#endif
}