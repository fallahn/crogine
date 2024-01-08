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

#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/RenderTarget.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/graphics/Texture.hpp>

#include <array>

namespace cro
{
    /*!
    \brief A render buffer with 1 or more colour targets and a depth buffer.
    MultiRenderTextures (or MRTs) have multiple colour targets and can be used
    as a G-Buffer for techniques such as deferred rendering or screen-space
    post processes like SSAO or depth of field. Unlike the standard RenderTexture
    the colour buffers are 32 bit floating point format for higher precision
    when rendering data such as normal or vertex position information, EXCEPT
    for attachment 0 which is a regular 8bit colour buffer.
    MultiRenderTextures are not available on mobile platforms, and RenderTexture
    should be used instead.
    */
    class CRO_EXPORT_API MultiRenderTexture final : public RenderTarget, public Detail::SDLResource
    {
    public:
        /*!
        \brief Constructor.
        By default MultiRenderTextures are in an invalid state until create() has been called
        at least once.
        To draw to an MRT first call its clear() function, which then activates
        it as the current target. All drawing operations will then be performed on it
        until display() is called. Both clear AND display() *must* be called either side of
        drawing to prevent undefined results.
        */
        MultiRenderTexture();
        ~MultiRenderTexture();

        MultiRenderTexture(const MultiRenderTexture&) = delete;
        MultiRenderTexture(MultiRenderTexture&&) noexcept;
        const MultiRenderTexture& operator = (const MultiRenderTexture&) = delete;
        MultiRenderTexture& operator = (MultiRenderTexture&&) noexcept;

        /*!
        \brief Creates (or recreates) the multi render texture
        \param width Width of the texture to create.
        \param height Height of the texture.
        \param colourCount The number of colour buffers to create. This should be at least 1
        and less than getMaxAttachments(). For instances where only a depth buffer is required
        use DepthTexture. When only one target is required and lower precision is acceptable,
        use RenderTexture.
        \returns true on success, else returns false
        */
        bool create(std::uint32_t width, std::uint32_t height, std::size_t colourCount = 2);

        /*!
        \brief Returns the current size in pixels of the texture (zero if not yet created)
        */
        glm::uvec2 getSize() const override;

        /*!
        \brief Clears the texture ready for drawing
        This should be called to activate the texture as the current drawing target.
        From then on all drawing operations will be applied to the MultiRenderTexture until display()
        is called. For every clear() call there must be exactly one display() call. This
        also attempts to save and restore any existing viewport, while also applying its
        own during rendering.
        This version clears all attached buffers with the given colour.
        */
        void clear(Colour colour = cro::Colour::Black);

        /*!
        \brief Clears the buffer with the array of given colours
        MultiRenderTextures can clear each attached buffer with a different colour.
        If too few colours are passed then only as many buffers as there are
        colours are cleared. If too many are passed only as many buffers are present
        will be cleared with the given colours in the vector up to the buffer count.
        */
        void clear(const std::vector<Colour>& colours);

        /*!
        \brief This must be called once for each call to clear to properly validate
        the final state of the texture. Failing to do this will result in undefined
        behaviour.
        */
        void display();

        /*!
        \brief Returns true if the render texture is available for drawing.
        If create() has not yet been called, or previously failed then this
        will return false
        */
        bool available() const { return m_fboID != 0; }

        /*!
        \brief Returns the colour attachment at 0 as a default 8 bit texture
        */
        const cro::Texture& getTexture() const { return m_defaultTexture; }

        /*!
        \brief Returns the texture ID of a texture wrapped 
        in a handle which can be bound to material uniforms.
        \param index The index of the texture to return.
        */
        TextureID getTexture(std::size_t index) const;

        /*!
        \brief Returns the texture ID of the Depth texture wrapped
        in a handle which can be bound to material uniforms.
        */
        TextureID getDepthTexture() const;

        /*!
        \brief Returns the maximum number of colour attachments available
        */
        std::int32_t getMaxAttaments() const;

    private:
        std::uint32_t m_fboID;
        mutable std::int32_t m_maxAttachments;
        
        std::vector<std::uint32_t> m_textureIDs;
        std::uint32_t m_depthTextureID;

        glm::uvec2 m_size;
        std::array<float, 4u> m_lastClearColour = {};

        std::uint32_t getFrameBufferID() const override { return m_fboID; }

        Texture m_defaultTexture; //this is used for colour rendering on target 0
    };
}