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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <array>

namespace cro
{
    /*!
    \brief Used to trigger callbacks when events occur in the Input's area
    */
    class CRO_EXPORT_API UIInput final
    {
    public:
        enum CallbackID
        {
            Enter = 1,
            Exit,
            ButtonDown,
            ButtonUp,
            Motion,
            Selected,
            Unselected,
            Count
        };

        FloatRect area; //! < area activated by the mouse cursor, in local coords
        bool active = false; //! < this is not the same as enabled!!
        bool enabled = true; //! < enables or disables this input temporarily
        std::array<std::uint32_t, CallbackID::Count> callbacks{};
        std::int32_t ID = -1;

        /*!
        \brief Sets the group of UIInput controls to which
        this component belongs.
        By default all UIInput components are added to
        group 0. When creating multiple menus it is sometimes
        advantageous to activate smaller groups of components
        at a time. Use UISystem::setActiveGroup() to control
        which group of UIInputs currently receive input.
        \see UISystem::setActiveGroup()
        */
        void setGroup(std::size_t group)
        {
            m_previousGroup = m_group;
            m_group = group;
            m_updateGroup = true;
        }

        /*!
        \brief Returns the ID of the group to which the input is currently assigned
        \see setGroup()
        */
        std::size_t getGroup() const { return m_group; }

        /*!
        \brief Defines the order in which components in a group are selected.
        By default this is set based on the order in which components
        are added to a group.
        */
        void setSelectionIndex(std::size_t index)
        {
            m_selectionIndex = index;
            m_updateGroup = true;
        }

        /*!
        \brief Returns the current selection index assigned to the UIInput
        */
        std::size_t getSelectionIndex() const { return m_selectionIndex; }

        /*!
        \brief Override the default selection index of the next UIInput to be selected
        \param right The next index to select when pressing right
        \param down The next index to select when pressing down
        */
        void setNextIndex(std::size_t right, std::size_t down = std::numeric_limits<std::size_t>::max())
        {
            m_neighbourIndices[1] = right;
            m_neighbourIndices[3] = down;
        }

        /*!
        \brief Override the default selection index of the next UIInput to be selected
        \param left The next index to select when pressing left
        \param up The next index to select when pressing up
        */
        void setPrevIndex(std::size_t left, std::size_t up = std::numeric_limits<std::size_t>::max())
        {
            m_neighbourIndices[0] = left;
            m_neighbourIndices[2] = up;
        }

    private:
        std::size_t m_previousGroup = 0;
        std::size_t m_group = 0;
        std::size_t m_selectionIndex = 0;
        bool m_updateGroup = true; //do order sorting by default

        struct Index final
        {
            enum
            {
                Left, Right, Up, Down,
                Count
            };
        };
        std::array<std::size_t, Index::Count> m_neighbourIndices =
        {
            std::numeric_limits<std::size_t>::max() ,
            std::numeric_limits<std::size_t>::max() ,
            std::numeric_limits<std::size_t>::max() ,
            std::numeric_limits<std::size_t>::max() 
        };

        cro::FloatRect m_worldArea; //cached by transform callback, ie dirty flag optimised

        friend class UISystem;
    };
}