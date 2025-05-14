/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

#include <array>

namespace cro
{
    /*!
    \brief A render buffer with only a depth component attached.
    DepthTextures are non-copyable objects but *can* be moved.
    */
    class CRO_EXPORT_API DepthTexture final : public RenderTarget, public Detail::SDLResource
    {
    public:
        /*!
        \brief Constructor.
        By default DepthTextures are in an invalid state until create() has been called
        at least once.
        To draw to a depth texture first call its clear() function, which then activates
        it as the current target. All drawing operations will then be performed on it
        until display() is called. Both clear AND display() *must* be called either side of
        drawing to prevent undefined results.
        */
        DepthTexture();
        ~DepthTexture();

        DepthTexture(const DepthTexture&) = delete;
        DepthTexture(DepthTexture&&) noexcept;
        const DepthTexture& operator = (const DepthTexture&) = delete;
        DepthTexture& operator = (DepthTexture&&) noexcept;

        /*!
        \brief Creates (or recreates) the depth texture
        \param width Width of the texture to create. This should be
        power of 2 on mobile platforms
        \param height Height of the texture.
        \param layers Number of layers to create for shadow cascades
        \returns true on success, else returns false
        */
        bool create(std::uint32_t width, std::uint32_t height, std::uint32_t layers = 1);

        /*!
        \brief Returns the current size in pixels of the depth texture (zero if not yet created)
        */
        glm::uvec2 getSize() const override;

        /*!
        \brief Clears the texture ready for drawing
        This should be called to activate the depth texture as the current draw target.
        From then on all drawing operations will be applied to the DepthTexture until display()
        is called. For every clear() call there must be exactly one display() call. This
        also attempts to save and restore any existing viewport, while also applying its
        own during rendering.
        \param layer Index of the layer to clear and render to. Must be less than getLayerCount()
        */
        void clear(std::uint32_t layer = 0);

        /*!
        \brief This must be called once for each call to clear to properly validate
        the final state of the depth texture. Failing to do this will result in undefined
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
        \brief Returns the texture ID wrapped in a handle which can be bound to
        material uniforms. Note that the depth texture is an array so requires
        a shader with a sampler2DArray uniform. To retrieve a single layer use
        getTexture(layerIndex) instead
        */
        TextureID getTexture() const;

        /*!
        \brief Returns a TextureID for the requested layer, if it exists
        Not available on macOS, so returns a null texture
        \param index Index of the layer to return
        */
        TextureID getTexture(std::uint32_t index) const;

        /*!
        \brief Returns the number of layers in the texture
        */
        std::uint32_t getLayerCount() const { return m_layerCount; }

    private:
        std::uint32_t m_fboID;
        std::uint32_t m_textureID;
        std::uint32_t m_colourID;
        glm::uvec2 m_size;
        std::uint32_t m_layerCount;

        std::array<float, 4u> m_lastClearColour = {};
        std::uint32_t getFrameBufferID() const override { return m_fboID; }

        //this manages the texture views for reading individual layers
        //however it requires at least GL 4.3 so isn't available on macOS
#ifndef __APPLE__
        std::vector<std::uint32_t> m_layerHandles;
#endif
        void updateHandles();

    };
}