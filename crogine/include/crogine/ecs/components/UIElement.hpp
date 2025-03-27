/*-----------------------------------------------------------------------

Matt Marchant 2025
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
#include <crogine/detail/glm/vec2.hpp>

#include <functional>

namespace cro
{
    class Entity;

    /*!
    \brief Updates the entity's position on screen when the
    active window is resized.
    Allows placing entities such as sprites or text in a relative
    position on the screen in normalised coords, plus an optional,
    specific offset in screen units.
    */
    struct CRO_EXPORT_API UIElement final
    {
        //<!Node type. This affects how the entity layout is applied
        enum Type
        {
            Default, //<! compatibility for existing projects.
            Position, //<! Parent-only nodes which are set to the relative position + scaled pixel offset
            Sprite, //<! parented to a position element, sprites are offset by absolute position and scaled if scaling is active
            Text //<! parented to a position element, text is not scaled, but the character size is, based on the default character size
        }type = Default;

        //<! if true then sprite nodes and text character size is scaled according to UIElementSystem::getViewScale()
        bool screenScale = false;

        //<! base character size for Text type elements
        std::uint32_t characterSize = 10u;

        //<!absolute in units offset from relative position
        glm::vec2 absolutePosition = glm::vec2(0.f); 
        //<!normalised relative to screen size (ignored by Sprite and Text types)
        glm::vec2 relativePosition = glm::vec2(0.f); 
        //<!z depth
        float depth = 0.f; 
        //<!optional callback. This is called before the component
        //is updated so that placement vars may be modified first
        //for example switching a sprite texture to a higher resolution
        //image depending on the current screen scale
        std::function<void(cro::Entity)> resizeCallback;

        UIElement() = default;
        UIElement(Type t, bool scale)
            : type(t), screenScale(scale) {}
    };
}