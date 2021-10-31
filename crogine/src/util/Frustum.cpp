/*-----------------------------------------------------------------------

Matt Marchant 2021
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


This is based on the work of Bruno Opsenica
https://bruop.github.io/improved_frustum_culling/

-----------------------------------------------------------------------*/

#include <crogine/util/Frustum.hpp>

#include <crogine/detail/glm/vec3.hpp>

#include <array>


using namespace cro;

namespace
{
    struct OBB final
    {
        glm::vec3 centre = glm::vec3(0.f);
        glm::vec3 extents = glm::vec3(0.f);
        std::array<glm::vec3, 3u> axes = { glm::vec3(0.f),glm::vec3(0.f), glm::vec3(0.f) };
    };
}

bool cro::Util::Frustum::visible(FrustumData frustum, const glm::mat4& viewSpaceTransform, Box aabb)
{
    //near, far
    float zNear = frustum.nearPlane;
    float zFar = frustum.farPlane;
    //half width, half height
    float xNear = frustum.nearRight;
    float yNear = frustum.nearTop;

    //so first thing we need to do is obtain the normal directions of our OBB by transforming 4 of our AABB vertices
    std::array<glm::vec3, 4u> corners = 
    {
        glm::vec3(aabb[0].x, aabb[0].y, aabb[0].z),
        {aabb[1].x, aabb[0].y, aabb[0].z},
        {aabb[0].x, aabb[1].y, aabb[0].z},
        {aabb[0].x, aabb[0].y, aabb[1].z}
    };

    //transform corners
    //this only translates to our OBB if our transform is affine
    for (auto& c : corners)
    {
        c = (viewSpaceTransform * glm::vec4(c, 1.f));
    }


    OBB obb;
    obb.axes =
    {
        corners[1] - corners[0],
        corners[2] - corners[0],
        corners[3] - corners[0]
    };
    //TODO is there a way to reduce the length() calls?
    obb.centre = corners[0] + 0.5f * (obb.axes[0] + obb.axes[1] + obb.axes[2]);
    obb.extents = glm::vec3{ glm::length(obb.axes[0]), glm::length(obb.axes[1]), glm::length(obb.axes[2]) };
    obb.axes[0] = obb.axes[0] / obb.extents.x;
    obb.axes[1] = obb.axes[1] / obb.extents.y;
    obb.axes[2] = obb.axes[2] / obb.extents.z;
    obb.extents *= 0.5f;

    {
        glm::vec3 M = { 0.f, 0.f, 1.f };
        float MoX = 0.f;
        float MoY = 0.f;
        float MoZ = 1.f;

        //projected center of our OBB
        float MoC = obb.centre.z;
        //projected size of OBB
        float radius = 0.f;

        for (auto i = 0; i < 3; i++)
        {
            //dot(M, axes[i]) == axes[i].z;
            radius += std::abs(obb.axes[i].z) * obb.extents[i];
        }
        float obbMin = MoC - radius;
        float obbMax = MoC + radius;

        float tau0 = zFar; // Since z is negative, far is smaller than near
        float tau1 = zNear;

        if (obbMin > tau1 || obbMax < tau0)
        {
            return false;
        }
    }

    {
        const std::array<glm::vec3, 4u> M = 
        {
            glm::vec3(zNear, 0.f, xNear), //left Plane
            { -zNear, 0.f, xNear }, //right plane
            { 0.f, -zNear, yNear }, //top plane
            { 0.f, zNear, yNear }, //bottom plane
        };

        for (auto m = 0u; m < M.size(); ++m)
        {
            float MoX = std::abs(M[m].x);
            float MoY = std::abs(M[m].y);
            float MoZ = M[m].z;
            float MoC = glm::dot(M[m], obb.centre);

            float obbRadius = 0.f;

            for (auto i = 0; i < 3; i++) 
            {
                obbRadius += std::abs(glm::dot(M[m], obb.axes[i])) * obb.extents[i];
            }
            float obbMin = MoC - obbRadius;
            float obbMax = MoC + obbRadius;

            float p = xNear * MoX + yNear * MoY;

            float tau0 = zNear * MoZ - p;
            float tau1 = zNear * MoZ + p;

            if (tau0 < 0.f)
            {
                tau0 *= zFar / zNear;
            }
            if (tau1 > 0.f)
            {
                tau1 *= zFar / zNear;
            }

            if (obbMin > tau1 || obbMax < tau0)
            {
                return false;
            }
        }
    }

    //OBB Axes
    {
        for (auto m = 0u; m < obb.axes.size(); ++m)
        {
            const glm::vec3& M = obb.axes[m];
            float MoX = std::abs(M.x);
            float MoY = std::abs(M.y);
            float MoZ = M.z;
            float MoC = glm::dot(M, obb.centre);

            float obbRadius = obb.extents[m];

            float obb_min = MoC - obbRadius;
            float obb_max = MoC + obbRadius;

            //frustum projection
            float p = xNear * MoX + yNear * MoY;
            float tau0 = zNear * MoZ - p;
            float tau1 = zNear * MoZ + p;

            if (tau0 < 0.f)
            {
                tau0 *= zFar / zNear;
            }
            if (tau1 > 0.f)
            {
                tau1 *= zFar / zNear;
            }

            if (obb_min > tau1 || obb_max < tau0)
            {
                return false;
            }
        }
    }

    //now let's perform each of the cross products between the edges
    //first R x A_i
    {
        for (auto m = 0u; m < obb.axes.size(); ++m)
        {
            const glm::vec3 M = { 0.f, -obb.axes[m].z, obb.axes[m].y };
            float MoX = 0.f;
            float MoY = std::abs(M.y);
            float MoZ = M.z;
            float MoC = M.y * obb.centre.y + M.z * obb.centre.z;

            float obbRadius = 0.f;
            for (auto i = 0; i < 3; ++i)
            {
                obbRadius += std::abs(glm::dot(M, obb.axes[i])) * obb.extents[i];
            }

            float obbMin = MoC - obbRadius;
            float obbMax = MoC + obbRadius;

            //frustum projection
            float p = xNear * MoX + yNear * MoY;
            float tau0 = zNear * MoZ - p;
            float tau1 = zNear * MoZ + p;
            if (tau0 < 0.f)
            {
                tau0 *= zFar / zNear;
            }
            if (tau1 > 0.f)
            {
                tau1 *= zFar / zNear;
            }

            if (obbMin > tau1 || obbMax < tau0)
            {
                return false;
            }
        }
    }

    //U x A_i
    {
        for (auto m = 0u; m < obb.axes.size(); ++m) 
        {
            const glm::vec3 M = { obb.axes[m].z, 0.f, -obb.axes[m].x };
            float MoX = std::abs(M.x);
            float MoY = 0.0f;
            float MoZ = M.z;
            float MoC = M.x * obb.centre.x + M.z * obb.centre.z;

            float obbRadius = 0.f;
            for (auto i = 0; i < 3; ++i)
            {
                obbRadius += std::abs(glm::dot(M, obb.axes[i])) * obb.extents[i];
            }

            float obbMin = MoC - obbRadius;
            float obbMax = MoC + obbRadius;

            //frustum projection
            float p = xNear * MoX + yNear * MoY;
            float tau0 = zNear * MoZ - p;
            float tau1 = zNear * MoZ + p;

            if (tau0 < 0.f)
            {
                tau0 *= zFar / zNear;
            }
            if (tau1 > 0.f)
            {
                tau1 *= zFar / zNear;
            }

            if (obbMin > tau1 || obbMax < tau0)
            {
                return false;
            }
        }
    }

    // Frustum Edges X Ai
    {
        for (auto j = 0u; j < obb.axes.size(); ++j)
        {
            const std::array<glm::vec3, 4u> M =
            {
                glm::cross({-xNear, 0.f, zNear}, obb.axes[j]), //left Plane
                glm::cross({ xNear, 0.f, zNear }, obb.axes[j]), //right plane
                glm::cross({ 0.f, yNear, zNear }, obb.axes[j]), //top plane
                glm::cross({ 0.f, -yNear, zNear }, obb.axes[j]) //bottom plane
            };

            for (auto m = 0u; m < M.size(); ++m)
            {
                float MoX = std::abs(M[m].x);
                float MoY = std::abs(M[m].y);
                float MoZ = M[m].z;

                static constexpr float epsilon = 1e-4f;
                if (MoX < epsilon && MoY < epsilon && std::abs(MoZ) < epsilon)
                {
                    continue;
                }

                float MoC = glm::dot(M[m], obb.centre);

                float obbRadius = 0.f;
                for (auto i = 0; i < 3; ++i)
                {
                    obbRadius += std::abs(dot(M[m], obb.axes[i])) * obb.extents[i];
                }

                float obbMin = MoC - obbRadius;
                float obbMax = MoC + obbRadius;

                //frustum projection
                float p = xNear * MoX + yNear * MoY;
                float tau0 = zNear * MoZ - p;
                float tau1 = zNear * MoZ + p;

                if (tau0 < 0.f)
                {
                    tau0 *= zFar / zNear;
                }
                if (tau1 > 0.f)
                {
                    tau1 *= zFar / zNear;
                }

                if (obbMin > tau1 || obbMax < tau0)
                {
                    return false;
                }
            }
        }
    }

    return true;
}