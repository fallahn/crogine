/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include "../../detail/GLCheck.hpp"

#include <crogine/ecs/components/Camera.hpp>

using namespace cro;

Camera::Camera() 
    : viewport      (0.f, 0.f, 1.f, 1.f),
    m_verticalFOV   (0.6f),
    m_aspectRatio   (1.f),
    m_nearPlane     (0.1f),
    m_farPlane      (150.f),
    m_orthographic  (false)
{
    glm::vec2 windowSize(App::getWindow().getSize());
    m_aspectRatio = windowSize.x / windowSize.y;
    m_projectionMatrix = glm::perspective(m_verticalFOV, m_aspectRatio, m_nearPlane, m_farPlane);

    m_passes[Pass::Final].m_cullFace = GL_BACK;
    m_passes[Pass::Final].m_planeMultiplier = -1.f;

    m_passes[Pass::Reflection].m_cullFace = GL_FRONT;
}

void Camera::setActivePass(std::uint32_t pass)
{
    CRO_ASSERT(pass <= Pass::Refraction, "Must be a valid Pass enum value");

    switch (pass)
    {
    default:
    case Pass::Final:
        m_passIndex = Pass::Final;
#ifdef PLATFORM_DESKTOP
        glCheck(glDisable(GL_CLIP_DISTANCE0));
#endif
        break;
    case Pass::Reflection:
        m_passIndex = Pass::Reflection;
#ifdef PLATFORM_DESKTOP
        glCheck(glEnable(GL_CLIP_DISTANCE0));
#endif
        break;
    case Pass::Refraction:
        //we'll be sharing the same matrices
        //and draw lists
        m_passIndex = Pass::Final;
#ifdef PLATFORM_DESKTOP
        glCheck(glEnable(GL_CLIP_DISTANCE0));
#endif
        break;
    }
}

const Camera::Pass& Camera::getPass(std::uint32_t pass) const
{
    CRO_ASSERT(pass <= Camera::Pass::Refraction, "Must be a value for Camera::Pass enum");
    switch (pass)
    {
    default:
    case Camera::Pass::Final:
    case Camera::Pass::Refraction:
        return m_passes[Pass::Final];
    case Camera::Pass::Reflection:
        return m_passes[Pass::Reflection];
    }
}

Camera::DrawList& Camera::getDrawList(std::uint32_t pass)
{
    CRO_ASSERT(pass <= Camera::Pass::Refraction, "Must be a value for Camera::Pass enum");
    switch (pass)
    {
    default:
    case Camera::Pass::Final:
    case Camera::Pass::Refraction:
        return m_passes[Pass::Final].drawList;
    case Camera::Pass::Reflection:
        return m_passes[Pass::Reflection].drawList;
    }
}

const Camera::DrawList& Camera::getDrawList(std::uint32_t pass) const
{
    CRO_ASSERT(pass <= Camera::Pass::Refraction, "Must be a value for Camera::Pass enum");
    switch (pass)
    {
    default:
    case Camera::Pass::Final:
    case Camera::Pass::Refraction:
        return m_passes[Pass::Final].drawList;
    case Camera::Pass::Reflection:
        return m_passes[Pass::Reflection].drawList;
    }
}

void Camera::setPerspective(float fov, float aspect, float nearPlane, float farPlane)
{
    m_projectionMatrix = glm::perspective(fov, aspect, nearPlane, farPlane);
    m_verticalFOV = fov;
    m_aspectRatio = aspect;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    m_orthographic = false;

    auto fovTan = std::tan(fov / 2.f);
    m_frustumData.farPlane = -farPlane;
    m_frustumData.nearPlane = -nearPlane;
    m_frustumData.nearRight = aspect * nearPlane * fovTan;
    m_frustumData.nearTop = nearPlane * fovTan;
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
{
    m_projectionMatrix = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    m_verticalFOV = -1.f;
    m_aspectRatio = (right - left) / (bottom - top);
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    m_orthographic = true;

    //TODO fix this
    m_frustumData.farPlane = -farPlane;
    m_frustumData.nearPlane = -nearPlane;
    m_frustumData.nearRight = m_aspectRatio * nearPlane;
    m_frustumData.nearTop = nearPlane;
}

void Camera::updateMatrices(const Transform& tx, float level)
{
    auto& finalPass = m_passes[Camera::Pass::Final];
    auto& reflectionPass = m_passes[Camera::Pass::Reflection];

    finalPass.viewMatrix = glm::inverse(tx.getWorldTransform());
    finalPass.viewProjectionMatrix = m_projectionMatrix * finalPass.viewMatrix;
    finalPass.m_aabb = Spatial::updateFrustum(finalPass.m_frustum, finalPass.viewProjectionMatrix);

    //TODO - disable this pass if the reflection buffer isn't created?
    reflectionPass.viewMatrix = glm::scale(finalPass.viewMatrix, glm::vec3(1.f, -1.f, 1.f));
    reflectionPass.viewMatrix = glm::translate(reflectionPass.viewMatrix, glm::vec3(0.f, level, 0.f));
    reflectionPass.viewProjectionMatrix = m_projectionMatrix * reflectionPass.viewMatrix;
    reflectionPass.m_aabb = Spatial::updateFrustum(reflectionPass.m_frustum, reflectionPass.viewProjectionMatrix);
}

glm::vec2 Camera::coordsToPixel(glm::vec3 worldPoint, glm::vec2 targetSize, std::int32_t passIdx) const
{
    CRO_ASSERT(passIdx > -1 && passIdx < Pass::Refraction, "");

    const auto& pass = m_passes[passIdx];

    glm::vec4 clipPos = pass.viewProjectionMatrix * glm::vec4(worldPoint, 1.f);
    
    //discard anything on or behind the view plane
    if (clipPos.w <= 0)
    {
        return glm::vec2(0.f);
    }
    
    glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;

    glm::vec2 windowSize = targetSize;
    
    glm::vec2 viewSize = windowSize;
    viewSize.x *= viewport.width;
    viewSize.y *= viewport.height;

    glm::vec2 windowPos = ((glm::vec2(ndcPos) + 1.f) / 2.f) * viewSize;
    windowPos.x += viewport.left * windowSize.x;
    windowPos.y += viewport.bottom * windowSize.y;
    
    return windowPos;
}

glm::vec3 Camera::pixelToCoords(glm::vec2 screenPosition, glm::vec2 targetSize)
{
    glm::uvec4 vp(targetSize.x * viewport.left, targetSize.y * viewport.bottom, targetSize.x * viewport.width, targetSize.y * viewport.height);

    //mouse coords are inverse in Y direction.
    screenPosition.y = targetSize.y - screenPosition.y;
    return glm::unProject(glm::vec3(screenPosition, 0.f), m_passes[Pass::Final].viewMatrix, m_projectionMatrix, vp);
}