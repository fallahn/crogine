/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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
#include <crogine/detail/Types.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/ecs/Entity.hpp>

namespace cro
{
    class ConfigObject;

    /*!
    \brief Message class

    The message class contains an ID used to identify
    the type of data contained in the message, and the
    message data itself. crogine uses some internal messaging
    types, so custom messages should have their IDs start at
    Message::Type::Count
    \see MessageBus
    */
    class CRO_EXPORT_API Message final
    {
        friend class MessageBus;
    public:
        using ID = std::int32_t;
        enum Type
        {
            AudioMessage = 0,
            UIMessage,
            WindowMessage,
            SceneMessage,
            StateMessage,
            ConsoleMessage,
            SystemMessage,
            SkeletalAnimationMessage,
            SpriteAnimationMessage,
            Count
        };

        /*!
        \brief Audio event message data
        */
        struct AudioEvent final
        {
            enum
            {
                Play,
                Pause,
                Stop,
                ChannelVolumeChanged
            }action;
        };
        /*!
        \brief Window event message. These
        repeat windows events from the event
        handler, such as focus changes or
        resize events.
        \see SDL_WindowEvent
        */
        struct WindowEvent final
        {
            std::uint32_t windowID = 0;
            std::uint8_t event = 0;
            std::int32_t data0 = 0;
            std::int32_t data1 = 0;
        };

        struct SceneEvent final
        {
            enum
            {
                EntityDestroyed
            }event = EntityDestroyed;
            std::uint32_t entityID = std::numeric_limits<std::uint32_t>::max();
        };

        /*!
        \brief Raised when there is a State
        change in the StateStack
        */
        struct StateEvent final
        {
            enum
            {
                Pushed,
                Popped,
                Cleared
            }action = Pushed;
            std::int32_t id = -1;
        };

        /*!
        \brief Raised when the Console is opened,
        closed, or a message is printed.
        */
        struct ConsoleEvent final
        {
            enum
            {
                Opened,
                Closed,
                LinePrinted,
                ConvarSet
            }type = Opened;
            
            enum Level
            {
                Info, Warning, Error
            }level = Info;

            ConfigObject* convar = nullptr;
        };

        /*!
        \brief Raised on system events such as full screen toggle
        */
        struct SystemEvent final
        {
            enum
            {
                FullScreenToggled
            }type = FullScreenToggled;
        };

        struct SkeletalAnimationEvent final
        {
            std::int32_t animationID = -1; //! animation which was playing when this event was raised
            std::int32_t userType = -1; //! < User assigned event ID
            glm::vec3 position = glm::vec3(0.f); //! < World position of the joint which raised this event
            Entity entity; //! < Entity which raised the event

            static constexpr std::int32_t Stopped = std::numeric_limits<std::int32_t>::max();
        };

        /*!
        \brief Raised when a Sprite animation frame is
        set that has an event ID > -1 assigned to it.
        */
        struct SpriteAnimationEvent final
        {
            std::int32_t userType = -1; //! < User assigned event ID
            Entity entity; //! < Entity which raised the event
        };

        ID id = -1;

        /*!
        \brief Returns the actual data contained in the message

        Using the ID of the message to determine the data type of the
        message, this function will return a reference to that data.
        It is important to request the correct type of data from the
        message else behaviour will be undefined.

        if(msg.id == Type::PhysicsMessage)
        {
        const PhysicsEvent& data = msg.getData<PhysicsEvent>();
        //do stuff with data
        }

        */
        template <typename T>
        const T& getData() const
        {
            //this isn't working on MSVC?
            //static_assert(std::is_trivially_constructible<T>::value && std::is_trivially_destructible<T>::value, "");
            CRO_ASSERT(sizeof(T) == m_dataSize, "size of supplied type is not equal to data size");
            return *static_cast<T*>(m_data);
        }

    private:
        void* m_data = nullptr;
        std::size_t m_dataSize = 0;
    };
}