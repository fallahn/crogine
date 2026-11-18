/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>

namespace cro
{
    /*!
    \brief Updates the geometry data of BillboardCollection components
    \see BillboardCollection
    */
    class CRO_EXPORT_API BillboardSystem final : public System, public Renderable
    {
    public:
        explicit BillboardSystem(MessageBus&);

        void process(float) override;

        /*!
        \brief Performs the default or custom grid culling based
        on the given cameraID for each billboard entity
        */
        void updateDrawList(Entity camera) override;

        /*!
        \brief Rather than actually render anything this picks the
        correct indexing list for each billboard using the camera
        ID and applies it to the model component for actual rendering.
        As such this system should always be placed between the
        CameraSystem and ModelRenderer systems when added to a Scene
        */
        void render(Entity camera, const RenderTarget&) override;

    private:


        //does nothing other than meet the expectations if the Renderable abstract
        void flushEntity(Entity) {}
    };
}
