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

#ifndef CRO_TEXT_HPP_
#define CRO_TEXT_HPP_

#include <crogine/Config.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <string>
#include <vector>

namespace cro
{
    class Font;

    /*!
    \brief Component to draw text.
    */
    class CRO_EXPORT_API Text final
    {
    public:
        Text();

        /*!
        \brief Constructor.
        \param font Font with which to draw text
        */
        explicit Text(const Font& font);

        /*!
        \brief Set the text's string
        */
        void setString(const std::string&);

        /*!
        \brief Sets the colour with which to render the text
        */
        void setColour(Colour);

        /*!
        \brief Returns the current line height of the text
        */
        float getLineHeight() const;

        /*!
        \brief Returns the local (pre transform) bounds
        */
        const FloatRect& getLocalBounds() const { return m_localBounds; }

    private:
        const Font* m_font;
        std::string m_string;
        Colour m_colour;
        uint8 m_dirtyFlags;
        enum Flags
        {
            Verts = 0x1, Colours = 0x2
        };

        struct Vertex final
        {
            glm::vec4 position;
            glm::vec4 colour;
            glm::vec2 UV;
        };
        std::vector<Vertex> m_vertices;
        int32 m_vboOffset; //starting index in parent VBO

        FloatRect m_localBounds;

        friend class TextRenderer;
    };
}

#endif //CRO_TEXT_HPP_