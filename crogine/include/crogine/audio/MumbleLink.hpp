/*
Uses Mumble's shared memory pattern to provide given listener and speaker
positions. Compatible with Mumble Link and Crosstalk to TeamSpeak

https://www.mumble.info/downloads/
https://github.com/thorwe/CrossTalk/wiki/Positional-Audio

Code based on https://www.mumble.info/documentation/developer/positional-audio/link-plugin/

*/

#pragma once

#include <crogine/Config.hpp>
#include <crogine/core/String.hpp>
#include <crogine/detail/glm/mat4x4.hpp>

#include <string>

namespace cro
{
    class CRO_EXPORT_API MumbleLink final
    {
    public:
        /*!
        \brief Constructor
        \param name This is used to set the name field of the link
        \param description This is used to set the description field in the
        target plugin when the link is connected.
        */
        MumbleLink(const cro::String& name, const cro::String& description);
        ~MumbleLink();

        MumbleLink(const MumbleLink&) = delete;
        MumbleLink(MumbleLink&&) = delete;

        MumbleLink& operator = (const MumbleLink&) = delete;
        MumbleLink& operator = (MumbleLink&&) = delete;

        /*!
        \brief Connect to and instance of Mumble or Crosstalk.
        \returns true on success or already connected, else false
        */
        bool connect();

        /*
        \brief Disconnects from Mumble or Crosstalk.
        Does nothing if not connected
        */
        void disconnect();

        /*!
        \brief Returns true if currently connected, else false
        */
        bool connected() const { return m_output != nullptr; }

        /*!
        \brief Sets the identity of this speaker to be shared with the link
        \param id A string containing the identity of this speaker.
        */
        void setIdentity(const cro::String& id);

        /*!
        \brief Sets the position and orientation of the speaker, in world coordinates
        \param tx A 4x4 matrix containing the speaker's world transform
        */
        void setSpeakerPosition(const glm::mat4& tx);

        /*!
        \brief Sets the position and orientation of the listener, in world coordinates
        \param tx A 4x4 matrix containing the listener's world transform
        */
        void setListenerPosition(const glm::mat4& tx);

        /*!
        \brief Push updates to the link
        This should be called once per frame
        */
        void update();

    private:

        struct LinkedMem* m_output;
        
        static constexpr std::size_t MaxDescription = 2048;
        std::basic_string<wchar_t> m_description;

        static constexpr std::size_t MaxIdentity = 256;
        std::basic_string<wchar_t> m_name;
        std::basic_string<wchar_t> m_identity;

#ifdef _WIN32
        void* m_fileHandle = nullptr;
#else
        char m_memname[MaxIdentity];
#endif
    };
}