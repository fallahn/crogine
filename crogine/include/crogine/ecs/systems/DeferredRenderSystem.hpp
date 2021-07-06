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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>

#include <map>

namespace cro
{
    /*
    \brief Deferred rendering system.
    This sytem renders PBR via a deferred system using the MRT (multi-render target)
    component attached to an active Camera. Deferred rendering supports PBR materials
    as well as forward-rendering (semi)transparent VertexLit materials using order
    independent depth sorting. If a Scene contains no PBR materials, then prefer the 
    ModelRenderer system to this one. This system is also unavailable on mobile platforms, 
    in which case only VertexLit materials are available with the ModelRenderer.

    The deferred renderer also implements SSAO, screen space reflection and screen space
    refraction. This can be enabled/disabled accordingly as the effects require higher-end
    hardware.
    */
    class CRO_EXPORT_API DeferredRenderSystem final : public System, public Renderable
    {
    public:
        explicit DeferredRenderSystem(MessageBus&);

        void updateDrawList(Entity camera) override;

        void render(Entity camera, const RenderTarget& target) override;

    private:

        struct SortData final
        {
            Entity entity; //model entity
            std::vector<std::int32_t> materialIDs; //index into the model submesh array
            float distanceFromCamera = 0.f; //sort criteria
        };

        struct VisibleList final
        {
            std::vector<SortData> deferred;
            std::vector<SortData> forward;
        };

        //one for each camera we encounter
        //camer holds a index into this
        std::map<std::uint32_t, VisibleList> m_visibleLists;
        std::uint32_t m_cameraCount;
    };
}