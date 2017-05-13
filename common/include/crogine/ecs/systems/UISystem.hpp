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

#ifndef CRO_UI_SYSTEM_HPP_
#define CRO_UI_SYSTEM_HPP_

#include <crogine/ecs/System.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <glm/mat4x4.hpp>

#include <functional>

namespace cro
{
    class CRO_EXPORT_API UISystem final : public System
    {
    public:
        //passes in the entity for whom the callback was triggered and a copy of the flags
        //which contain the input which triggered it. Use the Flags enum to find the input type
        using Callback = std::function<void(Entity, uint64 flags)>;

        explicit UISystem(MessageBus&);

        /*!
        \brief Used to pass in any events the system may be interested in
        */
        void handleEvent(const Event&);

        /*!
        \brief Performs processing
        */
        void process(Time) override;

        /*!
        \brief Message handler
        */
        void handleMessage(const Message&) override;

        /*!
        \brief Adds a callback.
        \returns ID of the callback. This should be used to assigned the callback
        to the relative callback slot of a UIInput component. eg:
        auto id = system.addCallback(cb);
        component.callbacks[UIInput::MouseEnter] = id;
        */
        uint32 addCallback(const Callback&);

        /*!
        \brief Input flags.
        Use these with the callback bitmask to find which input triggered it
        */
        enum Flags
        {
            RightMouse = 0x1,
            LeftMouse = 0x2,
            MiddleMouse = 0x4,
            Finger = 0x8
        };

    private:

        std::vector<Callback> m_callbacks;

        glm::vec2 m_previousEventPosition; //in screen coords
        glm::vec2 m_eventPosition;

        std::vector<Flags> m_downEvents;
        std::vector<Flags> m_upEvents;

        glm::uvec2 m_windowSize;

        //void setViewPort(int32, int32);
        glm::vec2 toScreenCoords(int32 x, int32 y); //converts screen coords
        glm::vec2 toScreenCoords(float, float); //converts normalised coords

        glm::mat4 getProjectionMatrix() const;
    };
}

#endif //CRO_UI_SYSTEM_HPP_