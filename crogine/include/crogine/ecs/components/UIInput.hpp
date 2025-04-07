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

#include <crogine/Config.hpp>
#include <crogine/detail/Assert.hpp>
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

        Use this when adding the component to a single group,
        or setting the first group of many, as it overwrites
        any existing groups. Further group assignments should be done
        with addToGroup()
        \see UISystem::setActiveGroup()
        */
        void setGroup(std::size_t group)
        {
            CRO_ASSERT(group < 32, "");
            m_previousGroup = m_group;
            m_group = (1 << group);
            m_updateGroup = true;
        }

        /*!
        \brief Returns the ID of the group to which the input is currently assigned
        \see setGroup()
        Note: since the addition of addToGroup() this will return the group ID flags
        OR'd together. Test for a specific bit to find out which groups are assigned.
        */
        std::size_t getGroup() const { return m_group; }

        /*!
        \brief Add this input to one or more groups in the UIInputSystem.
        This input will become active when UIInputSystem::setActiveGroup()
        is called with one of the grou IDs to which this input is assigned.
        */
        void addToGroup(std::size_t group)
        {
            CRO_ASSERT(group < 32, "");
            
            //only do this ifwe're not already expecting an update
            //from, say, setGroup() otherwise we're flagging groups
            //as assigned when they aren't yet.
            if (!m_updateGroup)
            {
                m_previousGroup = m_group;
            }
            m_group |= (1 << group);
            m_updateGroup = true;
        }

        /*!
        \brief Removes this input from a group with the given ID, if it is
        assigned, else does nothing.
        \see addToGroup()
        */
        void removeFromGroup(std::size_t group)
        {
            CRO_ASSERT(group < 32, "");
            if (!m_updateGroup)
            {
                m_previousGroup = m_group;
            }
            m_group &= ~(1 << group);
            m_updateGroup = true;
        }


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

        /*!
        \brief Returns the right and down indices of this component, set by setNextIndex()
        */
        std::array<std::size_t, 2u> getNextIndex() const
        {
            return { m_neighbourIndices[1], m_neighbourIndices[3] };
        }

        /*!
        \brief Returns the left and up indices of this component, set by setPrevIndex()
        */
        std::array<std::size_t, 2u> getPrevIndex() const
        {
            return { m_neighbourIndices[0], m_neighbourIndices[2] };
        }

        /*!
        \breif If called during a Select or Unselect event returns true if
        the event was triggered by a mouse enter or exit event
        */
        bool wasMouseEvent() const { return m_wasMouseEvent; }

    private:
        static constexpr auto DefaultGroup = (1 << 0);
        std::uint32_t m_previousGroup = DefaultGroup;
        std::uint32_t m_group = DefaultGroup;
        std::size_t m_selectionIndex = 0;
        bool m_updateGroup = true; //do order sorting by default

        bool m_wasMouseEvent = false;

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