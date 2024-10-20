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

#include <crogine/core/StateStack.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/detail/Assert.hpp>

using namespace cro;

StateStack::StateStack(State::Context context)
    : m_context (context),
    m_messageBus(context.appInstance.getMessageBus())
{

}

//public
void StateStack::unregisterState(StateID id)
{
    if (m_factories.count(id) != 0)
    {
        m_factories.erase(id);
    }
}

void StateStack::handleEvent(const cro::Event& evt)
{
    for (auto i = m_stack.rbegin(); i != m_stack.rend(); ++i)
    {
        if (!(*i)->handleEvent(evt)) break;
    }
}

void StateStack::handleMessage(const Message& msg)
{
    for (auto& s : m_stack)
    {
        s->handleMessage(msg);
    }

    //cached states need to know if we were resized
    if (msg.id == Message::WindowMessage)
    {
        for (auto& [_,s] : m_stateCache)
        {
            //if the state is in use (ie on the stack)
            //it'll already have this message
            if (!s->m_inUse)
            {
                s->handleMessage(msg);
            }
        }
    }
}

void StateStack::simulate(float dt)
{
    applyPendingChanges();
    for (auto i = m_stack.rbegin(); i != m_stack.rend(); ++i)
    {
        if (!(*i)->simulate(dt)) break;
    }
}

void StateStack::render()
{
    for (auto& s : m_stack) s->render();
}

void StateStack::pushState(StateID id)
{
    if ((empty() || m_stack.back()->getStateID() != id)
        && !changeExists(Action::Push, id))
    {
        m_pendingChanges.emplace_back(Action::Push, id, false);
    }
}

void StateStack::popState()
{
    m_pendingChanges.emplace_back(Action::Pop);
}

void StateStack::clearStates()
{
    if (changeExists(Action::Clear)) return;
    m_pendingChanges.emplace_back(Action::Clear);
}

bool StateStack::empty() const
{
    return m_stack.empty();
}

std::size_t StateStack::getStackSize() const
{
    return m_stack.size();
}

std::int32_t StateStack::getTopmostState() const
{
    if (!m_stack.empty())
    {
        return m_stack.back()->getStateID();
    }
    return -1;
}

//private
void StateStack::cacheState(StateID id)
{
    CRO_ASSERT(m_stateCache.count(id) == 0, "State is already cached");
    auto& state = m_stateCache.insert(std::make_pair(id, createState(id, true))).first->second;
    state->m_cached = true;

    //do one update to make sure the scene is properly initialised and ready
    //this fixes instances where resize callbacks aren't applied to cached
    //states which haven't been opened and have not fully registered their cameras
    state->simulate(0.f);
}

void StateStack::uncacheState(StateID id)
{
    if (m_stateCache.count(id))
    {
        m_stateCache.erase(id);
    }
}

bool StateStack::changeExists(Action action, std::int32_t id)
{
    return std::find_if(m_pendingChanges.begin(), m_pendingChanges.end(),
        [action, id](const PendingChange& pc)
    {
        return (id == pc.id && action == pc.action);
    }) != m_pendingChanges.end();
}

State::Ptr StateStack::createState(StateID id, bool isCached)
{
    CRO_ASSERT(m_factories.count(id) != 0, "State not registered with statestack");
    return m_factories.find(id)->second(isCached);
}

void StateStack::applyPendingChanges()
{
    m_activeChanges.swap(m_pendingChanges);
    for (const auto& change : m_activeChanges)
    {
        switch (change.action)
        {
        default: break;
        case Action::Push:
        {
            if (change.suspendPrevious && !m_stack.empty())
            {
                m_suspended.push_back(std::make_pair(change.id, std::move(m_stack.back())));  
                m_stack.pop_back();
            }
            auto* msg = m_messageBus.post<Message::StateEvent>(Message::StateMessage);
            msg->action = Message::StateEvent::Pushed;
            msg->id = change.id;

            //check if the requested state is already cached
            //by the currently active state
            if (m_stateCache.count(change.id) != 0)
            {
                m_stack.push_back(m_stateCache.at(change.id));
                m_stack.back()->m_inUse = true;
                m_stack.back()->onCachedPush();
            }
            else
            {
                m_stack.emplace_back(createState(change.id));
            }
        }
            break;
        case Action::Pop:
        {
            auto id = m_stack.back()->getStateID();

            auto* msg = m_messageBus.post<Message::StateEvent>(Message::StateMessage);
            msg->action = Message::StateEvent::Popped;
            msg->id = id;

            if (m_stack.back()->isCached())
            {
                m_stack.back()->onCachedPop();
            }

            m_stack.back()->m_inUse = false;
            m_stack.pop_back();

            if (!m_suspended.empty() && m_suspended.back().first == id)
            {
                //restore suspended state
                m_stack.push_back(std::move(m_suspended.back().second));
                m_suspended.pop_back();
            }
        }
            break;
        case Action::Clear:
            m_stack.clear();
            m_suspended.clear();
        {
            auto* msg = m_messageBus.post<Message::StateEvent>(Message::StateMessage);
            msg->action = Message::StateEvent::Cleared;
        }
            break;
        }
    }
    m_activeChanges.clear();
}
