/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/SDLResource.hpp>

#include <vector>

namespace cro
{
    class MessageBus;
    class Model;
    struct Camera;

    //don't export this, used internally.
    struct SortData final
    {
        int64 flags = 0;
        std::vector<int32> matIDs;
    };

    using MaterialPair = std::pair<Entity, SortData>;
    using MaterialList = std::vector<MaterialPair>;


    /*!
    \brief Used to draw scene Models.
    The system frustum-culls then renders any entities with a Model component
    in the scene. Note this only renders Models - Sprite and Text components
    have their own respective rendering systems.
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

    private:
        std::array<MaterialList, 2u> m_visibleEnts;

        static void applyProperties(const Material::Data&, const Model&, const Scene&, const Camera&);
        static void applyBlendMode(Material::BlendMode);
    };

    //just to keep it a bit more inline with the new render system naming
    using RenderSystem3D = ModelRenderer;
}