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

#include <crogine/ecs/System.hpp>

namespace cro
{
    class Font;

    /*!
    \brief Updates the geometry of Drawable2D components which
    are attached to entities with a Text component. Each entity
    is treated individually - for batching of text geometry a
    custom System can be defined which will combine multiple
    text instances into a single Drawable2D component.

    \see System, Text, RenderSystem2D
    */
    class CRO_EXPORT_API TextSystem final : public cro::System
    {
    public:
        /*!
        \brief Default constructor
        \param mb A reference to the active MessageBus
        */
        explicit TextSystem(MessageBus& mb);

        void process(float) override;

    private:

        //mechanism for marking fonts with updated pages
        //as read. This is double buffered to save iterating
        //over twice in the same update loop
        struct ReadPage final
        {
            const Font* f = nullptr;
            std::uint32_t charSize = 0;
        };
        std::vector<ReadPage> m_readPages;
        std::vector<ReadPage> m_pageBuffer;
    };
}