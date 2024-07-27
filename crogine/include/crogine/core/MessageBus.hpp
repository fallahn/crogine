/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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

#include <crogine/core/Message.hpp>

#include <vector>

#ifdef USE_PARALLEL_PROCESSING
#include <mutex>
#endif

namespace cro
{   
    /*!
    \brief System wide message bus for custom event messaging

    The app class contains an instance of the MessageBus which allows
    events and messages to be broadcast system wide, upon which objects
    entities and components can decide whether or not to act. Custom
    message type IDs can be added by extending the ID set starting
    at Message::Type::Count. EG

    enum MyMessageTypes
    {
    AlienEvent = Message::Type::Count,
    GhostEvent,
    BadgerEvent //etc...
    };
    */
    class CRO_EXPORT_API MessageBus final
    {
    public:
        MessageBus();
        ~MessageBus() = default;
        MessageBus(const MessageBus&) = delete;
        MessageBus(MessageBus&&) = delete;
        const MessageBus& operator = (const MessageBus&) = delete;
        MessageBus& operator = (MessageBus&&) = delete;

        /*!
        \brief Read and despatch all messages on the message stack

        Used internally by crogine
        */
        const Message& poll();
        /*!
        \brief Places a message on the message stack, and returns a pointer to the data

        The message data can then be filled in via the pointer. Custom message types can
        be defined via structs, which are then created on the message bus. Structs should
        contain only trivial data such as PODs and pointers to other objects.
        ATTEMPING TO PLACE LARGE OBJECTS DIRECTLY ON THE MESSAGE BUS IS ASKING FOR TROUBLE
        Custom message types should have a unique 32 bit integer ID which can be used
        to identify the message type when reading messages. Message data has a maximum
        size of 128 bytes.
        \param id Unique ID for this message type
        \returns Pointer to an empty message of given type.
        */
        template <typename T>
        T* post(Message::ID id)
        {
            if (!m_enabled) return static_cast<T*>((void*)m_pendingBuffer.data());

            static_assert(sizeof(T) < 128, "MEssage size limit is 128 bytes");

            const auto dataSize = sizeof(T);
            static const auto msgSize = sizeof(Message);
            //CRO_ASSERT(dataSize < 128, "message size exceeds 128 bytes"); //limit custom data to 128 bytes
            CRO_ASSERT(m_pendingBuffer.size() - (m_inPointer - m_pendingBuffer.data()) > (dataSize + msgSize), "buffer overflow " + std::to_string(m_pendingCount)); //make sure we have enough room in the buffer
            //CRO_WARNING(m_pendingBuffer.size() - (m_inPointer - m_pendingBuffer.data()) < 128, "Messagebus buffer is heavily contended!");


            //this is probably not ideal - the returned
            //message is probably modified outside of the lock
            //however what we're *really* protecting here
            //is creation of new messages on the bus
            //ALSO a lot of threads waiting on this will probably cause
            //a stall - if we're smart each thread/group of threads
            //would have their own mutexes and the message bus
            //would do a collection from all of them each frame
#ifdef USE_PARALLEL_PROCESSING
            std::scoped_lock l(m_mutex);
#endif

            Message* msg = new (m_inPointer)Message();
            m_inPointer += msgSize;
            msg->id = id;
            msg->m_dataSize = dataSize;
            msg->m_data = new (m_inPointer)T();
            m_inPointer += dataSize;

            m_pendingCount++;
            return static_cast<T*>(msg->m_data);
        }

        /*!
        \brief Returns true if there are no messages left on the message bus
        */
        bool empty();
        /*!
        \brief Returns the number of messages currently sitting on the message bus

        Useful for stat logging and debugging.
        */
        std::size_t pendingMessageCount() const;

        /*!
        \brief Disables the message bus.
        Used internally by crogine
        */
        void disable() { m_enabled = false; }

    private:

        std::vector<char> m_currentBuffer;
        std::vector<char> m_pendingBuffer;
        char* m_inPointer;
        char* m_outPointer;
        std::size_t m_currentCount;
        std::size_t m_pendingCount;

        bool m_enabled;
#ifdef USE_PARALLEL_PROCESSING
        std::mutex m_mutex;
#endif
    };
}