/*---------------------------------------------------------------------- -

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

---------------------------------------------------------------------- -*/

//creates a window to which to draw

#ifndef CRO_WINDOW_HPP_
#define CRO_WINDOW_HPP_

namespace cro
{
	class Window final
	{
	public:
		Window();
		~Window();
		Window(const Window&) = delete;
		Window(const Window&&) = delete;
		Window& operator = (const Window&) = delete;
		Window& operator = (const Window&&) = delete;

		void create();
		void clear();
		void display();

		bool pollEvent();

	private:

	};
}

#endif //CRO_WINDOW_HPP_