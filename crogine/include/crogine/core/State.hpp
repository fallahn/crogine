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

#include <crogine/Config.hpp>
#include <crogine/core/App.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/core/Message.hpp>

#include <cstdlib>
#include <memory>

namespace cro
{
    class Window;
    class App;
    class StateStack;
    class Time;

    using StateID = std::int32_t;

    /*!
    \brief Abstract base class for states.

    States, when used in conjunction with the state stack, are used
    to encapsulate game states, such as menus or pausing mode.
    Concrete states should provide a unique ID using the StateID type.
    */
    class CRO_EXPORT_API State
    {
    public:
        using Ptr = std::shared_ptr<State>;

        /*!
        \brief Context containing information about the current
        application instance.
        //TODO deprecate this as members are globally accessible
        */
        struct CRO_EXPORT_API Context
        {
            App& appInstance;
            Window& mainWindow;
            Context(App& a, Window& w) : appInstance(a), mainWindow(w) {}
        };

        /*!
        \brief Constructor

        \param StateStack Instance of the state to which this state will belong
        \param Context Copy of the current application's Context
        */
        State(StateStack&, Context);
        virtual ~State();

        State(const State&) = delete;
        State(const State&&) = delete;
        State& operator = (const State&) = delete;
        State& operator = (const State&&) = delete;

        /*!
        \brief Receives window events as handed down from the StateStack.
        \returns true if the event should be passed on through the stack, else false.
        For example; when displaying a pause menu this function should return false
        in order to consume events and prevent them from being passed on to the game
        state residing underneath.
        */
        virtual bool handleEvent(const cro::Event&) = 0;

        /*!
        \brief Receives system messages handed down from the StateStack.
        All states receive all messages regardless.
        */
        virtual void handleMessage(const cro::Message&) = 0;

        /*!
        \brief Executes one step of the game's simulation.
        All game logic should be performed under this function.
        \param dt The amount of time passed since this
        function was last called.
        \returns true if remaining states in the state should be simulated
        else returns false. 
        \see handleEvent()
        */
        virtual bool simulate(float dt) = 0;

        /*!
        \brief Calls to rendering systems should be performed here.
        */
        virtual void render() = 0;

        /*!
        \brief Returns the unique ID of concrete states.
        */
        virtual StateID getStateID() const = 0;

    protected:
        /*!
        \brief Request a state with the given ID is pushed on to
        the StateStack to which this state belongs.
        */
        void requestStackPush(StateID);

        /*!
        \brief Requests that the state currently on the top of
        the stack to which this state belongs is popped and destroyed.
        */
        void requestStackPop();

        /*!
        \brief Requests that all states in the StateStack to which this
        state belongs are removed.
        */
        void requestStackClear();

        /*!
        \brief Returns a copy of this state's current Context
        */
        Context getContext() const;

        /*!
        \brief Returns the number of active states on this state's StateStack
        */
        std::size_t getStateCount() const;

        /*!
        \brief Shortcut for posting a message on the active message bus
        \see MessageBus::post()
        */
        template <typename T>
        T* postMessage(std::int32_t id)
        {
            return m_context.appInstance.getMessageBus().post<T>(id);
        }

        /*!
        \brief Creates a cached state with the given ID
        When sub-states such as pause menus require being already
        loaded, pre-caching them here will prepare them for use.
        Cached states are automatically unloaded. This should usually
        be done *after* any loading in this state is complete.
        */
        void cacheState(StateID);

        /*!
        \brief Returns true if this State has been cached.
        Cached states should NOT call Window::loadResources()
        as this is likely already active from the state which
        cached this one.
        */
        bool isCached() const { return m_cached; }


        /*!
        \brief Called on cached states when they are pushed onto the stack
        Can be used to reset the state for example, as if it were created
        as new, but without reloading all resources
        */
        virtual void onCachedPush() {}


        /*!
        \brief Called immediately before a cached state is popped from the stack.
        Use this to (optionally) tidy up any game state within the state itself.
        */
        virtual void onCachedPop() {}


    private:
        StateStack& m_stack;
        Context m_context;
        bool m_cached;
        bool m_inUse; //only relevant if this is a cached state

        std::vector<std::int32_t> m_cachedIDs;
        friend class StateStack;
    };
}