/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#define PL_MPEG_IMPLEMENTATION

#include "VideoPlayer.hpp"

#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <string>

namespace
{
	const std::string ShaderVertex =
R"(
ATTRIBUTE vec2 a_position;
ATTRIBUTE vec2 a_texCoord0;
ATTRIBUTE vec4 a_colour;

uniform mat4 u_worldMatrix;
uniform mat4 u_projectionMatrix;

VARYING_OUT vec2 v_texCoord;
VARYING_OUT vec4 v_colour;

void main()
{
    gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
    v_texCoord = a_texCoord0;
    v_colour = a_colour;
})";


	//shader based on example at https://github.com/phoboslab/pl_mpeg
	const std::string ShaderFragment =
		R"(
uniform sampler2D u_texture; //Y component but renamed to match SimpleQuad expectations
uniform sampler2D u_textureCB;
uniform sampler2D u_textureCR;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

	const mat4 rec601 = 
		mat4(
			1.16438,  0.00000,  1.59603, -0.87079,
			1.16438, -0.39176, -0.81297,  0.52959,
			1.16438,  2.01723,  0.00000, -1.08139,
			0.0, 0.0, 0.0, 1.0
			);

void main()
{
	float y = TEXTURE(u_texture, v_texCoord).r;
	float cb = TEXTURE(u_textureCB, v_texCoord).r;
	float cr = TEXTURE(u_textureCR, v_texCoord).r;

	FRAG_OUT = vec4(y, cb, cr, 1.0) * rec601 * v_colour;
})";

}

//C senor.
void videoCallback(plm_t* mpg, plm_frame_t* frame, void* user)
{
	auto* videoPlayer = static_cast<VideoPlayer*>(user);
	videoPlayer->updateTexture(videoPlayer->m_y.getGLHandle(), &frame->y);
	videoPlayer->updateTexture(videoPlayer->m_cb.getGLHandle(), &frame->cb);
	videoPlayer->updateTexture(videoPlayer->m_cr.getGLHandle(), &frame->cr);
}


VideoPlayer::VideoPlayer()
	: m_plm				(nullptr),
	m_timeAccumulator	(0.f),
	m_frameTime			(0.f),
	m_state				(State::Stopped)
{
	if (!m_shader.loadFromString(ShaderVertex, ShaderFragment))
	{
		LogW << "Failed creating shader for video renderer" << std::endl;
	}
	else
	{
		//only need to set these once as we fix the layout (see update buffer)
		glUseProgram(m_shader.getGLHandle());
		glUniform1i(m_shader.getUniformID("u_texture"), 0);
		glUniform1i(m_shader.getUniformID("u_textureCR"), 1);
		glUniform1i(m_shader.getUniformID("u_textureCB"), 2);
	}
}

VideoPlayer::~VideoPlayer()
{
	if (m_plm)
	{
		stop();

		plm_destroy(m_plm);
	}
}

bool VideoPlayer::loadFromFile(const std::string& path)
{
	//remove existing file first
	if (m_state == State::Playing)
	{
		stop();
	}	
	
	if (m_plm)
	{
		plm_destroy(m_plm);
		m_plm = nullptr;
	}	
	
	
	if (m_shader.getGLHandle() == 0)
	{
		LogE << "Unable to open file " << cro::FileSystem::getFileName(path) << ": shader not loaded";
		return false;
	}

	if (!cro::FileSystem::fileExists(path))
	{
		LogE << "Unable to open file " << path << ": file not found" << std::endl;
		return false;
	}


	//load the file
	m_plm = plm_create_with_filename(path.c_str());

	if (!m_plm)
	{
		LogE << "Failed creating video player instance (incompatible file?)" << cro::FileSystem::getFileName(path) << std::endl;
		return false;
	}


	auto width = plm_get_width(m_plm);
	auto height = plm_get_height(m_plm);
	auto frameRate = plm_get_framerate(m_plm);

	if (width == 0 || height == 0 || frameRate == 0)
	{
		LogE << cro::FileSystem::getFileName(path) << ": invalid file properties" << std::endl;
		plm_destroy(m_plm);
		m_plm = nullptr;

		return false;
	}

	
	m_frameTime = 1.f / frameRate;

	m_y.create(width, height, cro::ImageFormat::A); //creates a single channel tex, but this is overwritten by out custom update anyway.
	m_cr.create(width, height, cro::ImageFormat::A);
	m_cb.create(width, height, cro::ImageFormat::A);
	m_outputBuffer.create(width, height, false);

	m_quad.setTexture(m_y);
	m_quad.setShader(m_shader);

	plm_set_video_decode_callback(m_plm, videoCallback, this);

	//TODO enable audio
	plm_set_audio_enabled(m_plm, FALSE);

	return true;
}

void VideoPlayer::update(float dt)
{
	m_timeAccumulator += dt;

	static constexpr float MaxTime = 1.f;
	if (m_timeAccumulator > MaxTime)
	{
		m_timeAccumulator = 0.f;
	}

	if (m_plm)
	{
		CRO_ASSERT(m_frameTime > 0, "");
		while (m_timeAccumulator > m_frameTime)
		{
			m_timeAccumulator -= m_frameTime;

			if (m_state == State::Playing)
			{
				plm_decode(m_plm, m_frameTime);

				updateBuffer();

				if (plm_has_ended(m_plm))
				{
					stop();
				}
			}
		}
	}
}

void VideoPlayer::play()
{
	if (m_plm == nullptr)
	{
		LogE << "No video file loaded " << std::endl;
		return;
	}

	if (m_frameTime == 0)
	{
		return;
	}

	if (m_state == State::Playing)
	{
		return;
	}

	m_timeAccumulator = 0.f;
	m_state = State::Playing;
}

void VideoPlayer::pause()
{
	if (m_state == State::Playing)
	{
		m_state = State::Paused;
	}
}

void VideoPlayer::stop()
{
	if (m_state != State::Stopped)
	{
		m_state = State::Stopped;

		if (m_plm)
		{
			//rewind the file
			plm_seek(m_plm, 0, FALSE);

			//clear the buffer else we repeat the last frame
			m_outputBuffer.clear();
			m_outputBuffer.display();
		}
	}
}

//private
void VideoPlayer::updateTexture(std::uint32_t textureID, plm_plane_t* plane)
{
	CRO_ASSERT(textureID != 0, "");
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, plane->width, plane->height, 0, GL_RED, GL_UNSIGNED_BYTE, plane->data);
}

void VideoPlayer::updateBuffer()
{
	//remaining shader setup is handled by SimpleQuad
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_cr.getGLHandle());
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_cb.getGLHandle());

	m_outputBuffer.clear();
	m_quad.draw();
	m_outputBuffer.display();
}