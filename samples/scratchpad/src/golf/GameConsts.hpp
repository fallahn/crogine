/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/graphics/Colour.hpp>
#include <crogine/util/Constants.hpp>

#include <cstdint>

static constexpr float CameraStrokeHeight = 2.6f;
static constexpr float CameraPuttHeight = 0.3f;
static constexpr float CameraStrokeOffset = 5.1f;
static constexpr float CameraPuttOffset = 0.8f;
static constexpr float FOV = 60.f * cro::Util::Const::degToRad;

static constexpr float BallPointSize = 1.4f;

static constexpr float MaxHook = -0.25f;
static constexpr float KnotsPerMetre = 1.94384f;
static constexpr float HoleRadius = 0.053f;