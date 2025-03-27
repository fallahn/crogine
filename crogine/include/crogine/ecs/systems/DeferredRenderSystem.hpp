/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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
#include <crogine/graphics/Shader.hpp>

#include <vector>

namespace cro
{
    class EnvironmentMap;

    /*
    \brief Deferred rendering system.
    This system renders PBR via a deferred system using the MRT (multi-render target)
    component attached to an active Camera. Deferred rendering supports PBR materials
    as well as forward-rendering (semi)transparent VertexLit materials using order
    independent depth sorting. NON-PBR MATERIALS WILL ALWAYS APPEAR SLIGHTY TRANSPARENT.
    
    If a Scene contains no PBR materials, then prefer the 
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
        ~DeferredRenderSystem();

        DeferredRenderSystem(const DeferredRenderSystem&) = delete;
        DeferredRenderSystem(DeferredRenderSystem&&) = delete;
        DeferredRenderSystem& operator = (const DeferredRenderSystem&) = delete;
        DeferredRenderSystem& operator = (DeferredRenderSystem&&) = delete;

        void updateDrawList(Entity camera) override;

        void render(Entity camera, const RenderTarget& target) override;

        /*!
        \brief Sets the environment map used by the renderer.
        This is usually the same as the EnvironmentMap set on the active Scene.
        This must be set for the system to render correctly.
        */
        void setEnvironmentMap(const EnvironmentMap&);

    private:

        struct SortData final
        {
            Entity entity; //model entity
            std::vector<std::int32_t> materialIDs; //index into the model sub-mesh array
            float distanceFromCamera = 0.f; //sort criteria
        };

        struct VisibleList final
        {
            std::vector<SortData> deferred;
            std::vector<SortData> forward;
        };

        //one for each camera we encounter
        std::vector<VisibleList> m_visibleLists;
        std::uint32_t m_cameraCount;
        std::vector<std::uint32_t> m_listIndices; //indexed by camera draw list index

        std::uint32_t m_deferredVao;
        std::uint32_t m_forwardVao;
        std::uint32_t m_vbo;
        Shader m_pbrShader;
        struct PBRUniformIDs final
        {
            enum
            {
                WorldMat,
                ProjMat,

                Diffuse,
                Normal,
                Mask,
                Position,

                LightDirection,
                LightColour,
                CameraWorldPosition,

                IrradianceMap,
                PrefilterMap,
                BRDFMap,

                InverseViewMat,
                LightProjMat, //actually multiplied with invView for shadow mapping
                ShadowMap,

                Count
            };
        };
        std::array<std::int32_t, PBRUniformIDs::Count> m_pbrUniforms;
        const EnvironmentMap* m_envMap;

        Shader m_oitShader;
        struct OITUniformIDs final
        {
            enum
            {
                WorldMat,
                ProjMat,

                Accum,
                Reveal,

                Count
            };
        };
        std::array<std::int32_t, OITUniformIDs::Count> m_oitUniforms;

        bool loadPBRShader();
        bool loadOITShader();
        void setupRenderQuad();

        void flushEntity(Entity) override {}
    };
}