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

#include "../detail/TextConstruction.hpp"
#include "../detail/GLCheck.hpp"

#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/RenderTarget.hpp>

using namespace cro;

SimpleText::SimpleText()
    : m_fontTexture     (nullptr),
    m_dirtyFlags        (DirtyFlags::All)
{
    setPrimitiveType(GL_TRIANGLES);
}

SimpleText::~SimpleText()
{
    if (m_context.font)
    {
        m_context.font->unregisterObserver(this);
    }
}

SimpleText::SimpleText(const Font& font)
    : SimpleText()
{
    setFont(font);
}

//public
void SimpleText::setFont(const Font& font)
{
    if (m_context.font != &font)
    {
        if (m_context.font)
        {
            m_context.font->unregisterObserver(this);
        }

        m_context.font = &font;
        m_context.font->registerObserver(this);
        m_dirtyFlags |= DirtyFlags::All;
    }
}

void SimpleText::setCharacterSize(std::uint32_t size)
{
    if (m_context.charSize != size)
    {
        m_context.charSize = size;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

void SimpleText::setVerticalSpacing(float spacing)
{
    if (m_context.verticalSpacing != spacing)
    {
        m_context.verticalSpacing = spacing;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

void SimpleText::setString(const String& str)
{
    if (m_context.string != str)
    {
        m_context.string = str;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

void SimpleText::setFillColour(Colour colour)
{
    if (m_context.fillColour != colour)
    {
        //hmm if we cached vertex data we
        //could just update the colour property rather
        //than rebuild the entire thing if only
        //the colour flag is set
        m_context.fillColour = colour;
        m_dirtyFlags |= DirtyFlags::ColourInner;
    }
}

void SimpleText::setOutlineColour(Colour colour)
{
    if (m_context.outlineColour != colour)
    {
        m_context.outlineColour = colour;
        m_dirtyFlags |= DirtyFlags::ColourOuter;
    }
}

void SimpleText::setOutlineThickness(float thickness)
{
    if (m_context.outlineThickness != thickness)
    {
        m_context.outlineThickness = thickness;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

void SimpleText::setShadowColour(Colour colour)
{
    if (m_context.shadowColour != colour)
    {
        m_context.shadowColour = colour;
        m_dirtyFlags |= DirtyFlags::ColourInner;
    }
}

void SimpleText::setShadowOffset(glm::vec2 offset)
{
    if (m_context.shadowOffset != offset)
    {
        m_context.shadowOffset = offset;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

void SimpleText::setBold(bool bold)
{
    if (m_context.bold != bold)
    {
        m_context.bold = bold;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

void SimpleText::setAlignment(std::int32_t alignment)
{
    CRO_ASSERT(alignment > -1 && alignment < Alignment::Null, "");
    if (m_context.alignment != alignment)
    {
        m_context.alignment = alignment;
        m_dirtyFlags = DirtyFlags::All;
    }
}

const Font* SimpleText::getFont() const
{
    return m_context.font;
}

std::uint32_t SimpleText::getCharacterSize() const
{
    return m_context.charSize;
}

float SimpleText::getVerticalSpacing() const
{
    return m_context.verticalSpacing;
}

const String& SimpleText::getString() const
{
    return m_context.string;
}

Colour SimpleText::getFillColour() const
{
    return m_context.fillColour;
}

Colour SimpleText::getOutlineColour() const
{
    return m_context.outlineColour;
}

float SimpleText::getOutlineThickness() const
{
    return m_context.outlineThickness;
}

Colour SimpleText::getShadowColour() const
{
    return m_context.shadowColour;
}

glm::vec2 SimpleText::getShadowOffset() const
{
    return m_context.shadowOffset;
}

bool SimpleText::getBold() const
{
    return m_context.bold;
}

FloatRect SimpleText::getLocalBounds()
{
    if (m_dirtyFlags)
    {
        m_dirtyFlags = 0;
        updateVertices();
    }
    return m_localBounds;
}

FloatRect SimpleText::getGlobalBounds()
{
    return getLocalBounds().transform(getTransform());
}

void SimpleText::draw(const glm::mat4& parentTransform)
{
    if (m_dirtyFlags)
    {
        m_dirtyFlags = 0;
        updateVertices();
    }
    drawGeometry(getTransform() * parentTransform);
}

//private
void SimpleText::updateVertices()
{
    if (m_context.font)
    {
        //do this first because the update below may
        //resize the texture and we want to know about it :)
        m_fontTexture = &m_context.font->getTexture(m_context.charSize);
        setTexture(*m_fontTexture);

        std::vector<Vertex2D> verts;
        m_localBounds = Detail::Text::updateVertices(verts, m_context);
        setVertexData(verts);
    }
}

void SimpleText::onFontUpdate()
{
    //m_dirtyFlags |= DirtyFlags::All;
    updateVertices();
}

void SimpleText::removeFont()
{
    m_context.font = nullptr;
    m_fontTexture = nullptr;
}