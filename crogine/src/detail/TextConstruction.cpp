/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "TextConstruction.hpp"

#include <crogine/ecs/components/Text.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

using namespace cro;

void Detail::Text::addQuad(std::vector<Vertex2D>& vertices, glm::vec2 position, Colour colour, const Glyph& glyph, glm::vec2 textureSize, float outlineThickness)
{
    //this might sound counter intuitive - but we're
    //making the characters top to bottom
    float left = glyph.bounds.left;
    float bottom = glyph.bounds.bottom;
    float right = glyph.bounds.left + glyph.bounds.width;
    float top = glyph.bounds.bottom + glyph.bounds.height;

    float u1 = static_cast<float>(glyph.textureBounds.left) / textureSize.x;
    float v1 = static_cast<float>(glyph.textureBounds.bottom) / textureSize.y;
    float u2 = static_cast<float>(glyph.textureBounds.left + glyph.textureBounds.width) / textureSize.x;
    float v2 = static_cast<float>(glyph.textureBounds.bottom + glyph.textureBounds.height) / textureSize.y;

    vertices.emplace_back(glm::vec2(position.x + left - outlineThickness, position.y + top + outlineThickness), glm::vec2(u1, v1), colour);
    vertices.emplace_back(glm::vec2(position.x + left - outlineThickness, position.y + bottom - outlineThickness), glm::vec2(u1, v2), colour);
    vertices.emplace_back(glm::vec2(position.x + right + outlineThickness, position.y + top + outlineThickness), glm::vec2(u2, v1), colour);

    vertices.emplace_back(glm::vec2(position.x + right + outlineThickness, position.y + top + outlineThickness), glm::vec2(u2, v1), colour);
    vertices.emplace_back(glm::vec2(position.x + left - outlineThickness, position.y + bottom - outlineThickness), glm::vec2(u1, v2), colour);
    vertices.emplace_back(glm::vec2(position.x + right + outlineThickness, position.y + bottom - outlineThickness), glm::vec2(u2, v2), colour);
}

FloatRect Detail::Text::updateVertices(std::vector<Vertex2D>& dst, TextContext& context)
{
    std::vector<Vertex2D> outlineVerts;
    std::vector<Vertex2D> shadowVerts;
    std::vector<Vertex2D> characterVerts;

    std::size_t rowStart = 0; //index of first vert in current row
    const auto realign = [&](float diff)
    {
        for (auto i = rowStart; i < characterVerts.size(); ++i)
        {
            characterVerts[i].position.x -= diff;

            if (context.outlineThickness != 0)
            {
                outlineVerts[i].position.x -= diff;
            }

            if (glm::length2(context.shadowOffset) != 0)
            {
                shadowVerts[i].position.x -= diff;
            }
        }
    };
    const auto eol = [&]()
    {
        if (rowStart == characterVerts.size())
        {
            //newline was at the end of the string
            //so do nothing
            return;
        }

        if (context.alignment == std::int32_t(cro::Text::Alignment::Centre))
        {
            float diff = std::floor((characterVerts.back().position.x - characterVerts[rowStart].position.x) / 2.f);
            realign(diff);
        }
        else if (context.alignment == std::int32_t(cro::Text::Alignment::Right))
        {
            float diff = std::floor(characterVerts.back().position.x - characterVerts[rowStart].position.x);
            realign(diff);
        }
    };

    const auto& texture = context.font->getTexture(context.charSize);
    float xOffset = static_cast<float>(context.font->getGlyph(L' ', context.charSize, context.bold, context.outlineThickness).advance);
    float yOffset = static_cast<float>(context.font->getLineHeight(context.charSize));
    float x = 0.f;
    float y = 0.f;// static_cast<float>(m_charSize);

    float minX = x;
    float minY = y;
    float maxX = 0.f;
    float maxY = 0.f;

    std::uint32_t prevChar = 0;
    const auto& string = context.string;
    for (auto i = 0u; i < string.size(); ++i)
    {
        std::uint32_t currChar = string[i];

        x += context.font->getKerning(prevChar, currChar, context.charSize);
        prevChar = currChar;

        //whitespace chars
        //might seem wasteful to render an empty glyph for spaces - 
        //however right aligned text breaks when there's a space at the end
        if (/*currChar == ' ' ||*/ currChar == '\t' || currChar == '\n')
        {
            minX = std::min(minX, x);
            minY = std::min(minY, y);

            switch (currChar)
            {
            default: break;
            /*case ' ':
                x += xOffset;
                break;*/
            case '\t':
                x += xOffset * 4.f; //4 spaces for tab suckas
                break;
            case '\n':
                y -= yOffset + context.verticalSpacing;
                x = 0.f;

                eol();

                rowStart = characterVerts.size();
                break;
            }

            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);

            continue; //skip quad for whitespace
        }

        //create the quads.
        auto addOutline = [&]()
        {
            const auto& glyph = context.font->getGlyph(currChar, context.charSize, context.bold, context.outlineThickness);

            //TODO mental jiggery to figure out why these cause an alignment
            //offset - although it's not important, it just means the localBounds
            //won't include any outline.
            /*float left = glyph.bounds.left;
            float top = glyph.bounds.bottom + glyph.bounds.height;
            float right = glyph.bounds.left + glyph.bounds.width;
            float bottom = glyph.bounds.bottom;*/

            //add the outline glyph to the vertices
            Detail::Text::addQuad(outlineVerts, glm::vec2(x, y), context.outlineColour, glyph, texture.getSize(), context.outlineThickness);

            /*minX = std::min(minX, x + left - m_outlineThickness);
            maxX = std::max(maxX, x + right + m_outlineThickness);
            minY = std::min(minY, y + bottom - m_outlineThickness);
            maxY = std::max(maxY, y + top + m_outlineThickness);*/
        };

        const auto& glyph = context.font->getGlyph(currChar, context.charSize, context.bold, 0.f);

        //if outline is larger, add first
        if (context.outlineThickness != 0)
        {
            addOutline();
        }
        else if (glm::length2(context.shadowOffset) != 0)
        {
            //add a shadow if only no outline
            Detail::Text::addQuad(shadowVerts, glm::vec2(x, y) + context.shadowOffset, context.shadowColour, glyph, texture.getSize());
        }
        Detail::Text::addQuad(characterVerts, glm::vec2(x, y), glyph.useFillColour ? context.fillColour : cro::Colour(1.f,1.f,1.f, context.fillColour.getAlpha()), glyph, texture.getSize());

        //only do this if not outlined
        //if (context.outlineThickness == 0)
        {
            float left = glyph.bounds.left;
            float top = glyph.bounds.bottom + glyph.bounds.height;
            float right = glyph.bounds.left + glyph.bounds.width;
            float bottom = glyph.bounds.bottom;

            minX = std::min(minX, x + left);
            maxX = std::max(maxX, x + right);
            minY = std::min(minY, y + bottom);
            maxY = std::max(maxY, y + top);
        }

        x += glyph.advance;
    }
    //update alignment if there is no newline at the end
    if (rowStart != characterVerts.size())
    {
        eol();
    }


    //ensures the outline/shadow is always drawn first
    outlineVerts.insert(outlineVerts.end(), shadowVerts.begin(), shadowVerts.end());
    outlineVerts.insert(outlineVerts.end(), characterVerts.begin(), characterVerts.end());
    dst.swap(outlineVerts);



    FloatRect localBounds;
    localBounds.left = minX;
    localBounds.bottom = minY;
    localBounds.width = maxX - minX;
    localBounds.height = maxY - minY;

    //check for alignment
    float offset = 0.f;
    if (context.alignment == std::int32_t(cro::Text::Alignment::Centre))
    {
        offset = localBounds.width / 2.f;
    }
    else if (context.alignment == std::int32_t(cro::Text::Alignment::Right))
    {
        offset = localBounds.width;
    }
    localBounds.left -= offset;

    return localBounds;
}