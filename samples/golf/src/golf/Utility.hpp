/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "CommonConsts.hpp"

#include <crogine/core/String.hpp>
#include <crogine/core/Keyboard.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/network/NetData.hpp>

#include <vector>
#include <cstring>

/*
first byte is string length in bytes, followed by string utf32 data
string is limited to MaxStringChars
*/
static inline std::vector<std::uint8_t> serialiseString(const cro::String& str)
{
    auto len = std::min(ConstVal::MaxStringChars, str.size());
    auto workingString = str.substr(0, len);

    std::uint8_t size = static_cast<std::uint8_t>(len * sizeof(std::uint32_t));
    std::vector<std::uint8_t> buffer(size + 1);
    buffer[0] = size;
    
    //if (size)
    {
        std::memcpy(&buffer[1], workingString.data(), size);
    }

    return buffer;
}

static inline cro::String deserialiseString(const net::NetEvent::Packet& packet)
{
    CRO_ASSERT(packet.getSize() > 0, "TODO we need better error handling");

    std::uint8_t stringSize = 0;
    std::memcpy(&stringSize, packet.getData(), 1);
    CRO_ASSERT(packet.getSize() == stringSize + 1, "");
    CRO_ASSERT(stringSize % sizeof(std::uint32_t) == 0, "");
    
    std::vector<std::uint32_t> buffer(stringSize / sizeof(std::uint32_t));
    std::memcpy(buffer.data(), static_cast<const std::uint8_t*>(packet.getData()) + 1, stringSize);

    cro::String str = cro::String::fromUtf32(buffer.begin(), buffer.end());
    return str;
}

static inline glm::mat4 lookFrom(const glm::vec3& eye, const glm::vec3 centre, const glm::vec3 up)
{
    //TODO this is still a lookAt - but surely wee can create a lookFrom
    //directly without having to inverse this all the time??

    const glm::vec3 f(glm::normalize(centre - eye));
    const glm::vec3 s(glm::normalize(glm::cross(f, up)));
    const glm::vec3 u(glm::cross(s, f));

    glm::mat4 Result(1);
    Result[0][0] = s.x;
    Result[1][0] = s.y;
    Result[2][0] = s.z;
    Result[0][1] = u.x;
    Result[1][1] = u.y;
    Result[2][1] = u.z;
    Result[0][2] = -f.x;
    Result[1][2] = -f.y;
    Result[2][2] = -f.z;
    Result[3][0] = -glm::dot(s, eye);
    Result[3][1] = -glm::dot(u, eye);
    Result[3][2] = glm::dot(f, eye);
    return Result;
}

//some stuff for quick debugging cameras / lights etc
struct KeyID final
{
    enum
    {
        Up, Down, Left, Right,
        CW, CCW,

        Count
    };
};
struct KeysetID final
{
    enum
    {
        Cube, Light
    };
};
static constexpr std::array keysets =
{
    std::array<std::int32_t, KeyID::Count>({SDLK_w, SDLK_s, SDLK_a, SDLK_d, SDLK_q, SDLK_e}),
    std::array<std::int32_t, KeyID::Count>({SDLK_HOME, SDLK_END, SDLK_DELETE, SDLK_PAGEDOWN, SDLK_INSERT, SDLK_PAGEUP})
};

static inline bool rotateEntity(cro::Entity entity, std::int32_t keysetID, float dt)
{
    const float rotationSpeed = dt;
    glm::vec3 rotation(0.f);
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Down]))
    {
        rotation.x -= rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Up]))
    {
        rotation.x += rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Right]))
    {
        rotation.y -= rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Left]))
    {
        rotation.y += rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::CW]))
    {
        rotation.z -= rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::CCW]))
    {
        rotation.z += rotationSpeed;
    }

    if (glm::length2(rotation) != 0
        && entity.isValid())
    {
        auto worldMat = glm::inverse(glm::mat3(entity.getComponent<cro::Transform>().getLocalTransform()));
        entity.getComponent<cro::Transform>().rotate(/*worldMat **/ cro::Transform::X_AXIS, rotation.x);
        entity.getComponent<cro::Transform>().rotate(worldMat * cro::Transform::Y_AXIS, rotation.y);
        entity.getComponent<cro::Transform>().rotate(/*worldMat **/ cro::Transform::Z_AXIS, rotation.z);

        return true;
    }
    return false;
}