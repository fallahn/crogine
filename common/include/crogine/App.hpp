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

#ifndef CRO_APP_HPP_
#define CRO_APP_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

#include <crogine/system/Colour.hpp>

namespace cro
{
	namespace Detail
	{
		class SDLResource;
	}
	class Time;

	/*!
	\brief Base class for crogine applications.
	One instance of this must exist before crogine may be used
	*/
	class CRO_EXPORT_API App
	{
	public:
		friend class Detail::SDLResource;
		App();
		virtual ~App();

		App(const App&) = delete;
		App(const App&&) = delete;
		App& operator = (const App&) = delete;
		App& operator = (const App&&) = delete;

		void run();

		void setClearColour(Colour);
		const Colour& getClearColour() const;

	protected:
		
		virtual void handleEvent(const Event&) = 0;
		//virtual void handleMessage() = 0;
		virtual void simulate(Time) = 0;
		virtual void render() = 0;

	private:

		Colour m_clearColour;

		static App* m_instance;
	};
}


#endif //CRO_APP_HPP_