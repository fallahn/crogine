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

#ifndef CRO_STATESTACK_HPP_
#define CRO_STATESTACK_HPP_

#include <crogine/State.hpp>

#include <vector>
#include <map>
#include <functional>

namespace cro
{
	class Window;
	class Time;

	/*!
	\brief Maintains a stack of active states.
	States are used to encapsulate different parts of an application or
	game, such as menus or the game state. States can be stacked upon each
	other, for example a pause state may be pushed on top of the game state
	so that it consumes events and input (in a pause menu) but makes sure the
	game state is not updated - hence paused - without losing its state altogether.

	A game or app derived from the App class usually has just one state stack
	to manage any customs states which are created by inheriting the abstract
	base class State. Custom states should be assigned a unique 32 bit ID
	and registered with the state stack.
	\see App::registerStates
	*/
	class CRO_EXPORT_API StateStack final
	{
	public:
		enum class Action
		{
			Push, Pop, Clear
		};

		explicit StateStack(State::Context);
		~StateStack() = default;

		StateStack(const StateStack&) = delete;
		StateStack(const StateStack&&) = delete;
		StateStack& operator = (const StateStack&) = delete;
		StateStack& operator = (const StateStack&&) = delete;

		/*!
		\brief Registers a state constructor with its StateID.
		This is done once at startup so that requesting a state
		be created by ID can be properly constructed.

		Optionally references to parameters required for construction
		may be passed after the ID. As these are *references* they must
		exist at least as long as the StateStack instance.
		*/
		template <typename T, typename... Args>
		void registerState(StateID id, Args&&... args)
		{
			static_assert(std::is_base_of<State, T>::value, "Must derive from State class");
			m_factories[id] = [this]()
			{
				return std::make_unique<T>(*this, m_constext, std::forward<Args>(args)...);
			};
		}

		/*!
		\brief Forwards window events to each active state on the stack,
		until the active state returns false or the end of the stack is reached.
		*/
		void handleEvent(const cro::Event&);

		/*!
		\brief Calls the simulate function of each active state,
		passing in the elapsed time, until the active state returns false,
		or the end of the stack is reached.
		*/
		void simulate(Time);

		/*!
		\brief Calls the render function on each state active on the stack
		*/
		void render();


		/*!
		\brief Pushes a new instance of a state with the given ID, assuming
		it has been registered with registerState()
		*/
		void pushState(StateID);

		/*!
		\brief Pops (and destroys) the topmost state from the stack
		*/
		void popState();

		/*!
		\brief Clears (and destroys) all states on the stack
		*/
		void clearStates();

		/*!
		\brief Returns whether or not the state stack is empty
		*/
		bool empty() const;

		/*!
		\brief Returns the number of states currently on the stack
		*/
		std::size_t getStackSize() const;
	private:

		struct PendingChange final
		{
			Action action;
			StateID id;
			bool suspendPrevious;
			PendingChange(Action a, StateID i = -1, bool suspend = false)
				: action(a), id(i), suspendPrevious(suspend) {}
		};

		std::vector<State::Ptr> m_stack;
		std::vector<std::pair<StateID, State::Ptr>> m_suspended;
		std::vector<PendingChange> m_pendingChanges;
		std::vector<PendingChange> m_activeChanges;
		State::Context m_context;
		std::map<StateID, std::function<State::Ptr()>> m_factories;
		
		State::Ptr createState(StateID);
		void applyPendingChanges();
	};
}

#endif //CRO_STATESTACK_HPP_