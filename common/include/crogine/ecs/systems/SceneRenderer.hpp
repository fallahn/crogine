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

#ifndef CRO_SCENE_RENDERER_HPP_
#define CRO_SCENE_RENDERER_HPP_

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

    //don't export this, used internally.
    struct SortData final
    {
        int64 flags = 0;
        std::vector<int32> matIDs;
    };

    using MaterialPair = std::pair<Entity, SortData>;
    using MaterialList = std::vector<MaterialPair>;


    /*!
    \brief Used to draw a given list of meshes.
    Provide an instance of this to systems such as the MeshSorter
    or LightSorter so that they may, in turn, update the entity lists
    required to render a scene. Note that this will not render any
    Sprite components - these should be drawn with SpriteRenderer
    */
    class CRO_EXPORT_API SceneRenderer final : public System, public Renderable, public Detail::SDLResource
    {
    public:
        /*!
        \brief Constructor.
        \param mb Refernce to the system message bus
        \param Default camera to use when rendering
        */
        SceneRenderer(MessageBus& mb, Entity defaultCamera);

        /*!
        \brief Sets the active camera when rendering
        \returns The existing camera which is being replaced.
        */
        Entity setActiveCamera(Entity camera);

        /*!
        \brief Returns a copy of the entity containing the active camera
        */
        Entity getActiveCamera() const;

        /*!
        \brief Supplies a list of entities to render.
        This would normally be supplied by a MeshSorter ECS system, but
        can come from any source such as a UI manager which can supply a list of entities.
        The vectors are swapped by this function so the vector being passed
        in will be cleared.
        */
        void setDrawableList(MaterialList& drawableList);

        /*!
        \brief Attempts to render the scene based on the current entity lists
        */
        void render() override;

    private:
        Entity m_activeCamera;
        MaterialList m_visibleEntities;
        //TODO list of lighting

        uint32 m_currentTextureUnit;
        void applyProperties(const Material::PropertyList&);

        void applyBlendMode(Material::BlendMode);
    };

}

#endif //CRO_SCENE_RENDERER_HPP_