/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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
#include <crogine/core/App.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/graphics/Spatial.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/MultiRenderTexture.hpp>
#include <crogine/graphics/DepthTexture.hpp>
#include <crogine/util/Frustum.hpp>

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <array>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <any>

namespace cro
{
    /*!
    \brief Render flags.
    Use these to filter renderable items which should be drawn for a
    specific pass. For example reflective plane geometry should only
    be rendered into the final buffer, not the reflection and refraction
    buffers.
    
    \begincode
    entity.getComponent<cro::Model>().setRenderFlags(ReflectionPlane | Flags::Minimap);

    camera.setRenderFlags(cro::Camera::Pass::Reflection, ReflectionPlane);
    \endcode

    Custom flags can be created for any renderable component starting
    at (1<<0) and incrementing (1<<x) up to the minimum value (1<<62)

    Flags can be OR'd together so that only specific sets of geometry
    are rendered, if they match the Camera's current flags:

    \begincode
    camera.setRenderFlags(cro::Camera::Pass::Final, (Flags::Minimap | Flags::Skybox));
    \endcode

    While these flags are active only geometry with the Minimap or
    Skybox flags will be rendered by the camera.
    */

    struct RenderFlags final
    {
        static constexpr std::uint64_t ReflectionPlane = ~(std::numeric_limits<std::uint64_t>::max() / 2);


        static constexpr std::uint64_t All = std::numeric_limits<std::uint64_t>::max();
    };


    /*!
    \brief Represents a camera within the scene.
    Use Scene::setActiveCamera() to use an entity with
    a camera component as the current view. The default
    output is a letter-boxed perspective camera with a 16:9
    ratio. This can easily be overridden with a custom
    projection matrix and viewport - usually set via a
    callback applied to Camera::resizeCallback

    \sa resizeCallback
    */
    struct CRO_EXPORT_API Camera final
    {
        /*!
        \brief Default constructor
        */
        Camera();


        /*!
        \brief Contains the View, ViewProjection and draw list for a given render pass.

        Cameras are updated for multiple passes for (optionally) rendering reflections
        and refraction maps which can be used for flat planes in a Scene such as water.

        The matrices for each pass are updated by the camera system, and the draw lists
        are updated by any Renderable system. It is up to the user to implement this in
        any custom renderable systems.

        When rendering passes for reflection or refraction mapping the active pass has to
        be selected on the Camera first, before then rendering the Scene

        \begincode
        camera.setActivePass(Camera::Pass::Reflection);
        camera.reflectionBuffer.clear();
        scene.draw(camera.reflectionBuffer);
        camera.reflectionBuffer.display();

        camera.setActivePass(Camera::Pass::Refraction);
        camera.refractionBuffer.clear();
        scene.draw(camera.refrationBuffer);
        camera.refractionBuffer.display();

        camera.setActivePass(Camera::Pass::Final);
        scene.draw(renderWindow);
        \endcode

        As each pass is rendered the RenderFlags for that specific pass is used to filter
        any drawables.

        Note that if any post process effects are active on the Scene that they should be
        disabled by first calling scene.setPostEnabled(false) and then re-enabling
        them for the final pass, to prevent post processes getting rendered into the
        reflection/refraction map buffers.
        Multiple passes can be combined with Camera::renderFlags to ensure specific
        sets of geometry are rendered during different passes.
        */
        struct Pass final
        {
            /*!
            \brief Render flags for this pass
            */
            std::uint64_t renderFlags = RenderFlags::All;

            /*!
            \brief View Matrix.
            This matrix is, by default, an identity matrix. This
            can be updated manually, although it is recommended that
            a Scene has an active CameraSystem added to it to automatically
            calculate both the View and ViewProjection matrices for
            all entities with a Camera component.
            */
            glm::mat4 viewMatrix = glm::mat4(1.f);

            /*!
            \brief The forward vector (the direction in which the pass looks)
            */
            glm::vec3 forwardVector = glm::vec3(0.f, 0.f, -1.f);

            /*!
            \brief ViewProjection matrix.
            \see viewMatrix
            */
            glm::mat4 viewProjectionMatrix = glm::mat4(1.f);

            enum
            {
                Final,
                Reflection,
                Refraction
            };

            /*!
            \brief Returns the camera frustum including any parent transformation,
            IE the frustum of the ViewProjection matrix. This requires an active
            CameraSystem in the Scene for the result to be up to date.
            */
            std::array<Plane, 6u> getFrustum() const { return m_frustum; }

            /*!
            \brief Returns the AABB of the current frustum.
            The Box returned is in world coordinates, encompassing the frustum.
            This is useful for world queries of the DynamicTree system to early
            cull objects which do not intersect the frustum. Submitting this
            to the DynamicTree system will return a list of entities which
            can then be further tested with the frustum culling during
            Scene::updateDrawLists()
            \see DynamicTreeSystem, Scene::updateDrawLists()
            \returns a Box encompassing the current frustum
            */
            Box getAABB() const { return m_aabb; }

            /*|
            \brief Returns the face to be culled in this pass, ie GL_FRONT or GL_BACK
            */
            std::uint32_t getCullFace() const { return m_cullFace; }

            /*!
            \brief Returns which direction the water plane should be facing.
            When implementing a custom renderer which uses the reflect/refract passes
            multiply the Scene's water plane by this value so that it points in
            the correct direction for clipping.
            */
            float getClipPlaneMultiplier() const { return m_planeMultiplier; }

        private:
            std::array<Plane, 6u> m_frustum = {};
            Box m_aabb;

            std::uint32_t m_cullFace = 0;
            float m_planeMultiplier = 1.f;

            friend struct Camera;
            friend class CameraSystem;
        };


        /*!
        \brief Sets the active render pass.
        By default this is set to 'Final'. If you are not rendering any
        additional passes then this is not necessary to call.
        \param pass Pass index to set.
        */
        void setActivePass(std::uint32_t pass);


        /*!
        \brief Sets the RenderFlags which should be used for the given pass
        \see RenderFlags
        */
        void setRenderFlags(std::uint32_t pass, std::uint64_t flags) { m_passes[pass].renderFlags = flags; }


        /*!
        \brief Returns a reference to the active pass.
        Use this to get the View and ViewProjection matrices for
        the current pass when rendering.
        */
        const Pass& getActivePass() const { return m_passes[m_passIndex]; }


        /*!
        \brief Returns the active Pass index
        This may be Final or Reflection - refraction passes use the same data
        as Final. This is the index used when returning the active Pass with
        getActivePass().
        */
        std::uint32_t getActivePassIndex() const { return m_passIndex; }


        /*!
        \brief Returns a reference to the requested pass.
        \param pass The Pass enum value requested.
        */
        const Pass& getPass(std::uint32_t pass) const;


        /*!
        \brief Returns the draw list index assigned to this camera.
        Renderable systems are expected to maintain their own list
        of visible entities - when this camera is passed into
        Renderable::updateDrawList() the index is used to map the
        camera to its respective draw list.
        */
        std::size_t getDrawListIndex() const { return m_drawListIndex; }


        /*!
        \brief Sets the projection matrix to a perspective projection
        \param fov y-axis field of view in radians
        \param aspect The aspect ratio of width to height
        \param nearPlane Distance to near plane
        \param farPlane Distance to far plane
        \param numSplits Number of splits to create if using cascaded shadow maps
        */
        void setPerspective(float fov, float aspect, float nearPlane, float farPlane, std::uint32_t numSplits = 1);


        /*!
        \brief Sets the projection matrix to an orthographic projection
        \param left Left side
        \param right Right side
        \param bottom Bottom side
        \param top Top side
        \param nearPlane Distance to near plane
        \param farPlane Distance to far plane
        \param numSplits Number of splits to create if using cascaded shadow maps
        */
        void setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane, std::uint32_t numSplits = 1);


        /*!
        \brief Returns true if set to an orthographic projection matrix
        */
        bool isOrthographic() const { return m_orthographic; }


        /*!
        \brief Returns a reference to the projection matrix
        By default it is constructed to a perspective matrix to match
        the current window size.
        */
        const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }


        /*!
        \brief Returns the 8 corners of the frustum bounds.
        The first 4 are the near plane, and second 4 are the far plane,
        starting top right, wound anti-clockwise.
        As the points are in local camera space altering the z-depth
        of the returned values will change the near or far plane values.
        This is updated each time setPerspective() or setOrthographic() is called.
        \returns Array of 8 points as glm::vec4, so can be easily multiplied
        with a transform matrix
        */
        const std::array<glm::vec4, 8u>& getFrustumCorners() const { return m_frustumCorners; }


        /*
        \brief Returns a vector of frustum corners arranged as a set of frustae
        created by splitting the camera's main frustum.
        This is used when rendering shadow map cascades, the number of which are specified
        when calling setPerspective() or setOrthographic(). Before splitting the
        far plane of the Camera's frustum is clamped to the maximum shadow distance
        \see setMaxShadowDistance()
        \see getFrustumCorners()
        */
        const std::vector<std::array<glm::vec4, 8u>>& getFrustumSplits() const { return m_frustumSplits; }


        /*!
        \brief Viewport.
        This is in normalised (0 - 1) coordinates to describe which part
        of the window this camera should render to. By default it is 0, 0, 1, 1
        which covers the entire screen. Changing this is useful when letter-boxing
        the output to maintain a fixed aspect ratio, or for rendering sub-areas of
        the screen such as a minimap or split-screen multiplayer.
        */
        FloatRect viewport;


        /*!
        \brief Resize callback.
        Optional callback automatically called by the camera's Scene if the 
        main window is resized. Useful for resizing viewports or letter-boxing
        2D views when the current window has changed aspect ratio. The callback
        takes the current camera as a parameter and returns void
        */
        std::function<void(Camera&)> resizeCallback;


        /*!
        \brief If this is set to false the CameraSystem will ignore
        this Camera component. Setting this to false on Camera components
        in a Scene which are currently not used (eg when switching between
        views) can improve performance as the Camera's draw list / frustum
        culling will not be updated unnecessarily.
        */
        bool active = true;


        /*!
        \brief If this camera is in a static Scene then this can be set to true.
        Static cameras will only update their draw list once, before automatically
        setting the active flag to false. Manually setting the active flag to true
        again will cause a single draw list update before returning the active flag
        to false. This can provide a performance increase on Scenes where the camera
        moves little to none at all between frames and saves on the cost of culling
        and sorting. Setting this back to false will cause draw list updates every
        frame when the camera is set to active.
        */
        bool isStatic = false;


        /*!
        \brief Target to use as this camera's reflection buffer.
        This is up to the user to initiate and draw to - by default
        the buffer remains uninitialised. Note that the visibility
        list for this buffer is only updated by the ModelRenderer
        and not the DeferredRenderSystem which prefers screen space
        reflections.
        \see Camera::Pass
        */
        RenderTexture reflectionBuffer;


        /*!
        \brief Target to use as this camera's refraction buffer.
        This is up to the user to initiate and draw to - by default
        the buffer remains uninitialised.
        \see Camera::Pass
        */
        RenderTexture refractionBuffer;



        /*
        \brief Depth map buffer used for rendering directional shadow maps.
        This must first be explicitly created at the desired resolution
        before shadows will be rendered with this camera. Once the buffer
        has been created a ShadowMapRenderer system must be active in the
        Scene to which this component belongs. The ShadowMapSystem will
        then automatically cull and render any entities with a Model and
        ShadowCaster component (and valid shadow material) to this buffer.
        The ModelRenderer will automatically bind and pass this buffer to
        any relevant shaders when the Scene is rendered with this camera.

        For notes on shadow casting and quality of shadow mapping see
        ShadowMapRenderer
        */
#ifdef PLATFORM_DESKTOP
        DepthTexture shadowMapBuffer;
#else
        RenderTexture shadowMapBuffer;
#endif

        /*!
        \brief Returns the viewProjection matrix for the Scene's
        directional light as calculated by the ShadowMapRenderer
        for this camera.
        */
        glm::mat4 getShadowViewProjectionMatrix() const { return m_shadowViewProjectionMatrices[0]; }


        /*!
        \brief Sets the maximum distance from this camera for
        rendering shadows.
        This is clamped to the camera's far plane distance. The
        value must be greater than the current near plane. This parameter
        is usually adjusted along with setShadowExpansion()
        \param distance The distance in world units from the camera
        to which to clamp shadow rendering.
        \see ShadowMapRenderer
        */
        void setMaxShadowDistance(float distance);


        /*!
        \brief Returns the current value set for maxShadowDistance()
        */
        float getMaxShadowDistance() const { return std::min(m_maxShadowDistance, m_farPlane); }


        /*!
        \brief Sets the amount of overshoot outside of the Camera
        frustum to expand each cascade.
        As shadow casting objects may fall outside of the Camera frustum
        the size of the light frustum can be expanded along the z axis (from the light)
        The default values is 0, but usually needs to be 1 unit or more, depending on
        the size of the Scene. This parameter is often adjusted along with
        setMaxShadowDistance()
        \param distance Positive value to add to the near and far plane of
        the light frustum.
        \see ShadowMapRenderer
        */
        void setShadowExpansion(float distance);


        /*!
        \brief Returns t he current value set for setShadowExpansion();
        */
        float getShadowExpansion() const { return m_shadowExpansion; }


        /*!
        \brief Returns the number of cascades this camera is currently
        enabled for, based on the numSplits parameter passed when calling
        setPerspective() or setOrthographic().
        */
        std::size_t getCascadeCount() const;


        /*!
        brief Returns the z depth, in view space, of each cascade split, ending
        with the far plane.
        */
        std::vector<float> getSplitDistances() const { return m_splitDistances; }


        /*!
        \brief Returns the vertical FOV in radians if the
        projection matrix is a perspective projection, else
        returns -1
        */
        float getFOV() const { return m_verticalFOV; }


        /*!
        \brief Returns the aspect ratio of the projection matrix
        */
        float getAspectRatio() const { return m_aspectRatio; }


        /*!
        \brief Returns the near plane value of the projection matrix
        */
        float getNearPlane() const { return m_nearPlane; }


        /*!
        \brief Returns the far plane value of the projection matrix
        */
        float getFarPlane() const { return m_farPlane; }


        /*!
        \brief Updates the Camera's view and projection matrices
        This is automatically called by the CameraSystem, but can be useful
        to force an update when needing to use coordsToPixel() when building
        a Scene.
        \param tx A reference to the Camera's entity transform
        \param reflectionLevel The level about which to set the reflection matrix
        */
        void updateMatrices(const Transform& tx, float reflectionLevel = 0.f);


        /*!
        \brief Returns the render target coordinates of the given
        3D world coordinates, as they would appear if rendered
        by this camera.
        \param worldPoint a vec3 containing world coords to convert
        \param targetSize The size of the target which would be rendered
        to. By default this is the size of the window but the size of
        a RenderTexture or DepthTexture may be provided.
        \param pass The render pass to use. For example the reflected
        render pass will return the reflected coordinates of the given
        point. Defaults to the Final pass. Note that the final pass 
        is the same as the Refraction pass.
        \see Pass
        returns vec2 in target coordinates.
        */
        glm::vec2 coordsToPixel(glm::vec3 worldPoint, glm::vec2 targetSize = cro::App::getWindow().getSize(), std::int32_t pass = Pass::Final) const;


        /*
        \brief Returns the world coordinates currently projected onto
        the given screen coordinates.
        If this gives unusual results, particularly with orthographic projections,
        makes sure that the camera's viewport is set correctly.
        */
        glm::vec3 pixelToCoords(glm::vec2 screenPosition, glm::vec2 targetSize = cro::App::getWindow().getSize()) const;


        /*!
        \brief Returns the FrustumData needed to test AABBs against
        this Camera's frustum, using cro::Util::Frustum::visible()
        */
        FrustumData getFrustumData() const { return m_frustumData; }


        /*!
        \brief Returns the view size if this is an orthographic camera
        else returns an empty FloatRect
        */
        FloatRect getViewSize() const { return m_orthographicView; }

#ifdef CRO_DEBUG_
        std::vector<std::array<glm::vec4, 8u>> lightCorners;
        std::vector<glm::vec3> lightPositions;
        glm::mat4 getShadowViewMatrix(std::size_t i) const { return m_shadowViewMatrices[i]; }
        const float* getShadowViewProjections() const { return reinterpret_cast<const float*>(m_shadowViewProjectionMatrices.data()); }
#endif

    private:

        std::array<Pass, 2u> m_passes = {}; //final pass and refraction pass share the same data
        std::uint32_t m_passIndex = Pass::Final;

        std::size_t m_drawListIndex = 0;

        friend class CameraSystem;

        glm::mat4 m_projectionMatrix = glm::mat4(1.f);
        float m_verticalFOV;
        float m_aspectRatio;
        float m_nearPlane;
        float m_farPlane;

        bool m_orthographic;
        FloatRect m_orthographicView;
        FrustumData m_frustumData;


        friend class ShadowMapRenderer;
        friend class ModelRenderer;

        std::array<glm::vec4, 8u> m_frustumCorners;
        std::vector<std::array<glm::vec4, 8u>> m_frustumSplits;
        void updateFrustumCorners(std::size_t);

        float m_maxShadowDistance;
        std::vector<glm::mat4> m_shadowViewProjectionMatrices;
        std::vector<glm::mat4> m_shadowViewMatrices;
        std::vector<glm::mat4> m_shadowProjectionMatrices;
        float m_shadowExpansion;
        std::vector<float> m_splitDistances;

        bool m_dirtyTx;
    };
}

/*!
\brief Comparison operator
Cameras are only moveable (because of the buffer textures), so it only makes sense that comparing cameras returns whether
or not we're comparing a reference to the same instance, particularly in sort/find cases
*/

static inline bool operator == (const cro::Camera& lhs, const cro::Camera& rhs)
{
    return &lhs == &rhs;
}

static inline bool operator != (const cro::Camera& lhs, const cro::Camera& rhs)
{
    return !(lhs == rhs);
}