/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_MODEL_RENDERER_HPP_
#define CRO_MODEL_RENDERER_HPP_

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
    in the scene. Note this only renders Models - Sprite and Texxt components
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
        void process(Time) override;

        /*!
        \brief Attempts to render the scene based on the current entity lists
        */
        void render(Entity) override;

    private:
        MaterialList m_visibleEntities;
        //TODO list of lighting

        uint32 m_currentTextureUnit;
        void applyProperties(const Material::PropertyList&, const Model&);

        void applyBlendMode(Material::BlendMode);
    };

}

#endif //CRO_MODEL_RENDERER_HPP_