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

#pragma once

#include "pl_mpeg.h"

#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

#include <vector>

/*
Video player class, which renders MPEG1 video to a texture using
pl_mpeg: https://github.com/phoboslab/pl_mpeg
*/

class VideoPlayer final
{
public:
	VideoPlayer();
	~VideoPlayer();

	VideoPlayer(const VideoPlayer&) = delete;
	VideoPlayer(VideoPlayer&&) noexcept = delete;
	//VideoPlayer(VideoPlayer&&) noexcept; //need to move plm pointer if we want to implement this

	VideoPlayer& operator = (const VideoPlayer&) = delete;
	VideoPlayer& operator = (VideoPlayer&&) noexcept = delete;
	//VideoPlayer& operator = (VideoPlayer&&) noexcept;

	/*!
	\brief Attempts to open an MPEG1 file.
	\returns true on success or false if the file doesn't
	exist or is not a valid MPEG1 file.
	*/
	bool loadFromFile(const std::string& path);

	/*!
	\brief Updates the decoding of the file, if a file is open.
	This is automatically locked to the frame rate of the video
	up to the maximum rate at which this is called, at which point
	frames will be skipped.
	\param dt The time since this function was last called

	hmmmmm - shame we can't spin this off into a thread, but OpenGL.
	*/
	void update(float dt);

	/*!
	\brief Starts the playback of the loaded file if available
	else does nothing.
	*/
	void play();

	/*!
	\brief Stops the current playback if the file is playing, else
	does nothing.
	*/
	void stop();

	/*!
	\brief Returns a reference to the texture to which the video is
	rendered.
	*/
	const cro::Texture& getTexture() const { return m_outputBuffer.getTexture(); }

private:

	plm_t* m_plm;

	float m_timeAccumulator;
	float m_frameTime;

	enum class State
	{
		Stopped, Playing
	}m_state;

	cro::Shader m_shader;

	cro::Texture m_y;
	cro::Texture m_cb;
	cro::Texture m_cr;
	cro::SimpleQuad m_quad;
	cro::RenderTexture m_outputBuffer;

	void updateTexture(std::uint32_t, plm_plane_t*);
	void updateBuffer();

	//because function pointers
	friend void videoCallback(plm_t*, plm_frame_t*, void*);
};