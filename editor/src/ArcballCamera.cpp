/*
The MIT License (MIT)

Copyright (c) 2016 Will Usher

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be included in all copies
or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "ArcBallCamera.hpp"

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <cmath>
#include <iostream>

namespace
{
    //project the point in [-1, 1] screen space onto the arcball sphere
    inline glm::quat screenToArcball(glm::vec2 p)
    {
        const float dist = glm::dot(p, p);
        //if we're on/in the sphere return the point on it
        if (dist <= 1.f)
        {
            return glm::quat(0.0, p.x, p.y, std::sqrt(1.f - dist));
        }
        else
        {
            //otherwise we project the point onto the sphere
            const glm::vec2 proj = glm::normalize(p);
            return glm::quat(0.0, proj.x, proj.y, 0.f);
        }
    }
}

ArcballCamera::ArcballCamera(glm::vec3 eye, glm::vec3 centre, glm::vec3 up)
    : m_centreTranslation   (1.f),
    m_translation           (1.f),
    m_rotation              (cro::Transform::QUAT_IDENTITY),
    m_camera                (1.f),
    m_inverseCamera         (1.f)
{
    const glm::vec3 dir = centre - eye;
    const glm::vec3 z = glm::normalize(dir);
    glm::vec3 x = glm::normalize(glm::cross(z, glm::normalize(up)));
    const glm::vec3 y = glm::normalize(glm::cross(x, z));
    x = glm::normalize(glm::cross(z, y));

    m_centreTranslation = glm::inverse(glm::translate(glm::mat4(1.f), centre));
    m_translation = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -glm::length(dir)));
    m_rotation = glm::normalize(glm::quat_cast(glm::transpose(glm::mat3(x, y, -z))));

    updateCamera();
}

void ArcballCamera::rotate(glm::vec2 prev, glm::vec2 current)
{
    //clamp mouse positions to stay in NDC
    current = glm::clamp(current, glm::vec2{-1.f, -1.f}, glm::vec2{1.f, 1.f});
    prev = glm::clamp(prev, glm::vec2{-1.f, -1.f}, glm::vec2{1.f, 1.f});

    const glm::quat ballCurrent = screenToArcball(current);
    const glm::quat ballPrev = screenToArcball(prev);

    m_rotation = ballCurrent * ballPrev * m_rotation;
    updateCamera();
}

void ArcballCamera::pan(glm::vec2 delta)
{
    const float zoom = std::abs(m_translation[3][2]);
    glm::vec4 motion(delta.x * zoom, delta.y * zoom, 0.f, 0.f);
    
    //find the panning amount in the world space
    motion = m_inverseCamera * motion;

    m_centreTranslation = glm::translate(glm::mat4(1.f), glm::vec3(motion)) * m_centreTranslation;
    updateCamera();
}

void ArcballCamera::zoom(float amount)
{
    const glm::vec3 motion(0.f, 0.f, amount);

    m_translation = glm::translate(glm::mat4(1.f), motion) * m_translation;
    updateCamera();
}

const glm::mat4 &ArcballCamera::getTransform() const
{
    return m_camera;
}

const glm::mat4 &ArcballCamera::getInvTransform() const
{
    return m_inverseCamera;
}

glm::vec3 ArcballCamera::getEyePosition() const
{
    return glm::vec3{m_inverseCamera * glm::vec4{0.f, 0.f, 0.f, 1.f}};
}

glm::vec3 ArcballCamera::getEyeDir() const
{
    return glm::normalize(glm::vec3{m_inverseCamera * glm::vec4{0.f, 0.f, -1.f, 0.f}});
}

glm::vec3 ArcballCamera::getUpVector() const
{
    return glm::normalize(glm::vec3{m_inverseCamera * glm::vec4{0.f, 1.f, 0.f, 0.f}});
}

void ArcballCamera::updateCamera()
{
    m_camera = m_translation * glm::mat4_cast(m_rotation) * m_centreTranslation;
    m_inverseCamera = glm::inverse(m_camera);
}