/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/BalancedTree.hpp>
#include <crogine/detail/SDLResource.hpp>

#include <vector>

namespace cro
{
    class MessageBus;
    struct Camera;

    //don't export this, used internally.
    struct SortData final
    {
        std::int64_t flags = 0;
        std::vector<std::int32_t> matIDs;
    };

    using MaterialPair = std::pair<Entity, SortData>;
    using MaterialList = std::vector<MaterialPair>;


    /*!
    \brief Used to draw scene Models.
    The system frustum-culls then renders any entities with a Model component
    in the scene. Note this only renders Models - Sprite and Text components
    are rendered with RenderSystem2D.
    */
    class CRO_EXPORT_API ModelRenderer final : public System, public Renderable
    {
    public:
        /*!
        \brief Constructor.
        \param mb Reference to the system message bus
        */
        explicit ModelRenderer(MessageBus& mb);

        /*!
        \brief Performs frustum culling and Material sorting by depth and blend mode
        */
        void updateDrawList(Entity) override;

        void process(float) override;

        /*!
        \brief Attempts to render the scene based on the current entity lists
        */
        void render(Entity, const RenderTarget&) override;

        /*!
        \brief Returns the size of the draw list of the given camera for the given pass
        Camera indices can be retrieved with Camera::getDrawlistIndex(). For example
        \begincode
            std::size_t visible = scene.getSystem<cro::ModelRenderer>()->getVisibleCount(camera.getDrawlistIndex(), cro::Camera::Pass::Final);
        \endcode
        \param cameraIndex The index of the camera's drawlist
        \param passIndex the ID of the pass to query
        */
        std::size_t getVisibleCount(std::size_t cameraIndex, std::int32_t passIndex = 0) const;

        struct VertexShaderID final
        {
            enum
            {
                Unlit, VertexLit, PBR
            };
        };

        /*!
        \brief Returns the default Vertex shader for 3D models as a string
        Use this when creating custom materials which only modify the fragment
        shader.
        \param type VertexShaderID type to return. Returns an empty string if this is invalid
        */
        static const std::string& getDefaultVertexShader(std::int32_t type);

        struct FragmentShaderID final
        {
            enum
            {
                Unlit, VertexLit, PBR
            };
        };
        /*!
        \brief Returns the requested default fragment shader as a string.
        Use this to get the default fragment shader for a specific material
        type when using custom vertex shaders.
        \param type FragmentShaderID type, Unlit, VertexLit or PBR. Other
        values will return an empty string.
        */
        static const std::string& getDefaultFragmentShader(std::int32_t type);

        void onEntityAdded(Entity) override;

        void onEntityRemoved(Entity) override;

    private:

        using DrawList = std::array<MaterialList, 2u>;
        std::vector<DrawList> m_drawLists;

        Mesh::IndexData::Pass m_pass;

        /*Detail::BalancedTree m_tree;
        bool m_useTreeQueries;*/

        void updateDrawListDefault(Entity);
        void updateDrawListBalancedTree(Entity);
        //std::vector<Entity> queryTree(Box) const;

        friend class DeferredRenderSystem;
        //these funcs are shared with above system - should probably be free funcs somewhere?
        static void applyProperties(const Material::Data&, const Model&, const Scene&, const Camera&);
        static void applyBlendMode(const Material::Data&);
    };

    //just to keep it a bit more inline with the new render system naming
    using RenderSystem3D = ModelRenderer;
}