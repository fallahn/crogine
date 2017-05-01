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

#ifndef CRO_STATE_HPP_
#define CRO_STATE_HPP_

#include <crogine/Config.hpp>
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

	using StateID = cro::int32;

	/*!
	\brief Abstract base class for states.

	States, when used in conjustion with the state stack, are used
	to encapsulate game states, such as menus or pausing mode.
	Concrete states should provide a unique ID using the StateID type.
	*/
	class CRO_EXPORT_API State
	{
	public:
		using Ptr = std::unique_ptr<State>;

		/*!
		\brief Context containing information about the current
		application instance.
		*/
		struct CRO_EXPORT_API Context
		{
			App& appInstance;
			Window& mainWindow;
			//TODO handle the default view of the window
			Context(App& a, Window& w) : appInstance(a), mainWindow(w) {}
		};

		/*!
		\brief Constructor

		\param StateStack Instance of th state state to which this state will belong
		\param Context Copy of the current application's Context
		*/
		State(StateStack&, Context);
        virtual ~State() = default;

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
        All states receive all messages regardlessly.
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
		virtual bool simulate(Time dt) = 0;

		/*!
		\brief Calls to rendering systems should be performed here.
		*/
		virtual void render() const = 0;

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

	private:
		StateStack& m_stack;
		Context m_context;
	};
}

#endif //CRO_STATE_HPP_