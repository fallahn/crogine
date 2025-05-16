/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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
#include <crogine/util/Matrix.hpp>

using namespace cro;

Camera::Camera() 
    : viewport          (0.f, 0.f, 1.f, 1.f),
    m_verticalFOV       (0.6f),
    m_aspectRatio       (1.f),
    m_nearPlane         (0.1f),
    m_farPlane          (150.f),
    m_orthographic      (false),
    m_maxShadowDistance (std::numeric_limits<float>::max()),
    m_shadowExpansion   (0.f),
    m_blurPasses        (0),
    m_dirtyTx           (true)
{
    glm::vec2 windowSize(App::getWindow().getSize());
    m_aspectRatio = windowSize.x / windowSize.y;
    m_projectionMatrix = glm::perspective(m_verticalFOV, m_aspectRatio, m_nearPlane, m_farPlane);

    m_passes[Pass::Final].m_cullFace = GL_BACK;
    m_passes[Pass::Final].m_planeMultiplier = -1.f;

    m_passes[Pass::Reflection].m_cullFace = GL_FRONT;

    std::fill(m_frustumCorners.begin(), m_frustumCorners.end(), glm::vec4(0.f));

    m_shadowProjectionMatrices.resize(1);
    m_shadowViewMatrices.resize(1);
    m_shadowViewProjectionMatrices.resize(1);

    m_frustumSplits.resize(1);
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

void Camera::setPerspective(float fov, float aspect, float nearPlane, float farPlane, std::uint32_t numSplits)
{
    m_projectionMatrix = glm::perspective(fov, aspect, nearPlane, farPlane);
    m_verticalFOV = fov;
    m_aspectRatio = aspect;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    m_orthographic = false;
    m_maxShadowDistance = std::min(farPlane, m_maxShadowDistance);

    m_orthographicView = { 0.f,0.f,0.f,0.f };

    auto fovTan = std::tan(fov / 2.f);
    m_frustumData.farPlane = -farPlane;
    m_frustumData.nearPlane = -nearPlane;
    m_frustumData.nearRight = aspect * nearPlane * fovTan;
    m_frustumData.nearTop = nearPlane * fovTan;

    CRO_ASSERT(numSplits > 0, "");
    updateFrustumCorners(numSplits);

    m_shadowProjectionMatrices.resize(numSplits);
    m_shadowViewMatrices.resize(numSplits);
    m_shadowViewProjectionMatrices.resize(numSplits);

    if (shadowMapBuffer.available()
        && shadowMapBuffer.getLayerCount() != numSplits)
    {
        auto size = shadowMapBuffer.getSize();
        shadowMapBuffer.create(size.x, size.y, numSplits);
    }
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane, std::uint32_t numSplits)
{
    m_projectionMatrix = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    m_verticalFOV = -1.f;
    m_aspectRatio = (right - left) / (bottom - top);
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    m_orthographic = true;
    m_maxShadowDistance = std::min(farPlane, m_maxShadowDistance);

    m_orthographicView = { left, bottom, right - left, top - bottom };

    //TODO fix this
    m_frustumData.farPlane = -farPlane;
    m_frustumData.nearPlane = -nearPlane;
    m_frustumData.nearRight = m_aspectRatio * nearPlane;
    m_frustumData.nearTop = nearPlane;

    m_frustumCorners =
    {
        //near
        glm::vec4(right, top, -nearPlane, 1.f),
        glm::vec4(left, top, -nearPlane, 1.f),
        glm::vec4(left, bottom, -nearPlane, 1.f),
        glm::vec4(right, bottom, -nearPlane, 1.f),

        //far
        glm::vec4(right, top, -farPlane, 1.f),
        glm::vec4(left, top, -farPlane, 1.f),
        glm::vec4(left, bottom, -farPlane, 1.f),
        glm::vec4(right, bottom, -farPlane, 1.f)
    };

    CRO_ASSERT(numSplits > 0, "");
    m_frustumSplits.resize(numSplits);
    m_splitDistances.resize(numSplits + 1);

    m_shadowProjectionMatrices.resize(numSplits);
    m_shadowViewMatrices.resize(numSplits);
    m_shadowViewProjectionMatrices.resize(numSplits);

    if (shadowMapBuffer.available()
        && shadowMapBuffer.getLayerCount() != numSplits)
    {
        auto size = shadowMapBuffer.getSize();
        shadowMapBuffer.create(size.x, size.y, numSplits);
    }

    //const float splitSize = (m_maxShadowDistance - nearPlane) / numSplits;

    float lastFar = m_maxShadowDistance;
    float offset = m_maxShadowDistance - nearPlane;
    std::vector<float> farPlanes(numSplits + 1);
    farPlanes[numSplits] = lastFar;
    for (auto i = numSplits - 1; i > 0; i--)
    {
        offset /= 3.f;
        lastFar -= (offset * 2.f);
        farPlanes[i] = lastFar;
    }
    farPlanes[0] = nearPlane;

    for (auto i = 0u; i < numSplits; ++i)
    {
        const float nearP = farPlanes[i];
        const float farP = farPlanes[i + 1];

        m_frustumSplits[i]=
        {
            //near
            glm::vec4(right, top, -nearP, 1.f),
            glm::vec4(left, top, -nearP, 1.f),
            glm::vec4(left, bottom, -nearP, 1.f),
            glm::vec4(right, bottom, -nearP, 1.f),

            //far
            glm::vec4(right, top, -farP, 1.f),
            glm::vec4(left, top, -farP, 1.f),
            glm::vec4(left, bottom, -farP, 1.f),
            glm::vec4(right, bottom, -farP, 1.f)
        };

        m_splitDistances[i] = m_frustumSplits[i][4].z;
    }
}

void Camera::setMaxShadowDistance(float distance)
{
    m_maxShadowDistance = std::min(m_farPlane, std::max(m_nearPlane + 0.05f, distance));
    updateFrustumCorners(m_frustumSplits.size());

    //TODO update the frustum splits if this is an ortho camera.
}

void Camera::setShadowExpansion(float distance)
{
    CRO_ASSERT(distance >= 0, "Must be positive");
    m_shadowExpansion = distance;
}

std::size_t Camera::getCascadeCount() const
{
    //this is just our current hard limit in the shader
    //if it comes to it set up the define MAX_CASCADES to be overridable
    CRO_ASSERT(m_frustumSplits.size() < 5, "");
    return m_frustumSplits.size();
}

void Camera::updateMatrices(const Transform& tx, float level)
{
    //TODO this doesn't take into account the edge cases
    //where clip level may change, but the camera transform
    //is not marked dirty

    //TODO this needs to be updated as soon as possible else coordsToPixel uses an out of date matrix

    /*if (tx.getDirty()
        || m_dirtyTx)*/
    {
        auto& finalPass = m_passes[Camera::Pass::Final];
        auto& reflectionPass = m_passes[Camera::Pass::Reflection];

        const auto worldTx = tx.getWorldTransform();
        finalPass.viewMatrix = glm::inverse(worldTx);
        finalPass.viewProjectionMatrix = m_projectionMatrix * finalPass.viewMatrix;
        finalPass.m_aabb = Spatial::updateFrustum(finalPass.m_frustum, finalPass.viewProjectionMatrix);
        finalPass.forwardVector = Util::Matrix::getForwardVector(worldTx);

        if (reflectionBuffer.available())
        {
            reflectionPass.viewMatrix = glm::scale(finalPass.viewMatrix, glm::vec3(1.f, -1.f, 1.f));
            reflectionPass.viewMatrix = glm::translate(reflectionPass.viewMatrix, glm::vec3(0.f, level, 0.f));
            reflectionPass.viewProjectionMatrix = m_projectionMatrix * reflectionPass.viewMatrix;
            reflectionPass.m_aabb = Spatial::updateFrustum(reflectionPass.m_frustum, reflectionPass.viewProjectionMatrix);
            reflectionPass.forwardVector = glm::reflect(finalPass.forwardVector, Transform::Y_AXIS);
        }

        m_dirtyTx = false;
    }
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

glm::vec3 Camera::pixelToCoords(glm::vec2 screenPosition, glm::vec2 targetSize) const
{
    glm::uvec4 vp(targetSize.x * viewport.left, targetSize.y * viewport.bottom, targetSize.x * viewport.width, targetSize.y * viewport.height);

    auto pixelCoords = screenPosition  * (targetSize / glm::vec2(cro::App::getWindow().getSize()));
    pixelCoords += glm::vec2(0.5f);

    glm::vec3 winCoords(pixelCoords.x, targetSize.y - pixelCoords.y, 0.f); //inverts mouse pos Y

    //TODO this triggers an abort trap 6 on mac. No idea why.
    if (!m_orthographic)
    {
        //ugh this is slow AF
        glCheck(glReadPixels(static_cast<std::int32_t>(winCoords.x), static_cast<std::int32_t>(winCoords.y), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winCoords.z));
    }

    return glm::unProject(winCoords, m_passes[Pass::Final].viewMatrix, m_projectionMatrix, vp);
}

//private
void Camera::updateFrustumCorners(std::size_t numSplits)
{
    const float tanHalfFOVY = std::tan(m_verticalFOV / 2.f);
    const float tanHalfFOVX = std::tan((m_verticalFOV * m_aspectRatio) / 2.f);

    float xNear = (m_nearPlane * tanHalfFOVX);
    float xFar = (m_farPlane * tanHalfFOVX);
    float yNear = (m_nearPlane * tanHalfFOVY);
    float yFar = (m_farPlane * tanHalfFOVY);

    m_frustumCorners =
    {
        //near
        glm::vec4(xNear, yNear, -m_nearPlane, 1.f),
        glm::vec4(-xNear, yNear, -m_nearPlane, 1.f),
        glm::vec4(-xNear, -yNear, -m_nearPlane, 1.f),
        glm::vec4(xNear, -yNear, -m_nearPlane, 1.f),

        //far
        glm::vec4(xFar, yFar, -m_farPlane, 1.f),
        glm::vec4(-xFar, yFar, -m_farPlane, 1.f),
        glm::vec4(-xFar, -yFar, -m_farPlane, 1.f),
        glm::vec4(xFar, -yFar, -m_farPlane, 1.f)
    };

    m_frustumSplits.resize(numSplits);
    m_splitDistances.resize(numSplits);

    //const float splitSize = (m_maxShadowDistance - m_nearPlane) / numSplits;

    float lastFar = m_maxShadowDistance;
    float offset = m_maxShadowDistance - m_nearPlane;
    std::vector<float> farPlanes(numSplits + 1);
    farPlanes[numSplits] = lastFar;
    for (auto i = numSplits - 1; i > 0; i--)
    {
        offset /= 4.f;
        lastFar -= (offset * 3.f);
        farPlanes[i] = lastFar;
    }
    farPlanes[0] = m_nearPlane;

    for (auto i = 0u; i < numSplits; ++i)
    {
        //const float nearPlane = m_nearPlane + (splitSize * i);
        //const float farPlane = nearPlane + splitSize;

        const float nearPlane = farPlanes[i];
        const float farPlane = farPlanes[i + 1];

        xNear = (nearPlane * tanHalfFOVX);
        xFar = (farPlane * tanHalfFOVX);
        yNear = (nearPlane * tanHalfFOVY);
        yFar = (farPlane * tanHalfFOVY);

        m_frustumSplits[i] = 
        {
            //near
            glm::vec4(xNear, yNear, -nearPlane, 1.f),
            glm::vec4(-xNear, yNear, -nearPlane, 1.f),
            glm::vec4(-xNear, -yNear, -nearPlane, 1.f),
            glm::vec4(xNear, -yNear, -nearPlane, 1.f),

            //far
            glm::vec4(xFar, yFar, -farPlane, 1.f),
            glm::vec4(-xFar, yFar, -farPlane, 1.f),
            glm::vec4(-xFar, -yFar, -farPlane, 1.f),
            glm::vec4(xFar, -yFar, -farPlane, 1.f)
        };

        m_splitDistances[i] = -farPlane;
    }
}