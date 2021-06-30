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

#include <crogine/graphics/postprocess/PostProcess.hpp>
#include <crogine/graphics/Shader.hpp>

#include <vector>

namespace cro
{
    class MultiRenderTexture;

    /*!
    \brief Screen space ambient occlusion.
    This effect is not available on mobile platforms.
    To render this post process the Scene must first be buffered to
    a MultiRenderTexture, using the GBuffer material pass of the
    ModelRenderer. Setup is approximately:

    Create models via ModelDefinition. This will automatically
    apply a GBuffer material to the model

    or

    Create a GBuffer material with a shader loaded with the ShaderResource
    using the cro::ShaderResource::BuiltIn::GBuffer flags. Apply the
    material to the Model component with Model::setGBufferMaterial()
    \see ShaderResource, Model

    Create a MultiRenderTexture for the GBuffer, with 2 colour
    targets. This can also be shared with other screen space post processes.
    \see MultiRenderTexture

    Render the Scene with post processes disabled and GBuffer materials
    active to the MultiRenderTexture:

    \begincode
        m_scene.getSystem<cro::ModelRenderer>().setRenderMaterial(cro::Model::MaterialPass::GBuffer);
        m_mrt.clear();
        m_scene.render(m_mrt, false);
        m_mrt.display();
        m_scene.getSystem<cro::ModelRenderer>().setRenderMaterial(cro::Model::MaterialPass::Final);
    \endcode

    Render the Scene normally:

    \begincode
        auto& rt = cro::App::getWindow();
        m_scene.render(rt);
        m_overlayScene.render(rt);
    \endcode

    */
    class PostSSAO final : public cro::PostProcess
    {
    public:
        /*!
        \brief Constructor
        \param mrt A reference to the MRT which contains the
        g-buffer data for vertex positions and normal vectors.
        \see Model, ModelSystem::setMaterialPass()
        */
        explicit PostSSAO(const MultiRenderTexture& mrt);

        ~PostSSAO();
        PostSSAO(const PostSSAO&) = delete;
        PostSSAO(PostSSAO&&) = delete;
        PostSSAO& operator = (const PostSSAO&) = delete;
        PostSSAO& operator = (PostSSAO&&) = delete;

        /*!
        \brief Automatically called by Scene::render()
        */
        void apply(const RenderTexture& source) override;

    private:
        const MultiRenderTexture& m_mrt;
        Shader m_ssaoShader;
        Shader m_blurShader;
        Shader m_blendShader;

        std::vector<glm::vec3> m_kernel;
        std::uint32_t m_noiseTexture;
        std::uint32_t m_ssaoTexture;

        std::uint32_t m_ssaoFBO;

        void createNoiseSampler();
        void bufferResized() override;
    };
}