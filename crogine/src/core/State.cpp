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

#include <crogine/core/StateStack.hpp>
#include <crogine/core/App.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

State::State(StateStack& stack, State::Context context)
    : m_stack   (stack),
    m_context   (context),
    m_cached    (false),
    m_inUse     (false)
{

}

State::~State()
{
    //remove any sub-states we cached.
    for (auto id : m_cachedIDs)
    {
        m_stack.uncacheState(id);
    }
}

//protected
void State::requestStackPush(StateID id)
{
    m_stack.pushState(id);
}

void State::requestStackPop()
{
    m_stack.popState();
}

void State::requestStackClear()
{
    m_stack.clearStates();
}

State::Context State::getContext() const
{
    return m_context;
}

std::size_t State::getStateCount() const
{
    return m_stack.getStackSize();
}

void State::cacheState(StateID id)
{
    CRO_ASSERT(id != getStateID(), "this won't end well.");
    m_stack.cacheState(id);
    m_cachedIDs.push_back(id);
}