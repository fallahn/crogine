/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "MyApp.hpp"

#include <crogine/system/Colour.hpp>

MyApp::MyApp()
{

}

//public
void MyApp::handleEvent(const cro::Event& evt)
{

}

#include <array>
void MyApp::simulate(cro::Time dt)
{
	static std::array<cro::Colour, 4u> colours =
	{
		cro::Colour(1.f, 0.f, 0.f),
		cro::Colour(1.f, 1.f, 0.f),
		cro::Colour(0.f, 1.f, 1.f),
		cro::Colour(1.f, 1.f, 1.f)
	};
	static std::size_t idx = 0;

	static cro::Clock timer;
	if (timer.elapsed().asSeconds() > 1)
	{
		timer.restart();
		setClearColour(colours[idx]);

		idx = (idx + 1) % colours.size();
	}
}

void MyApp::render()
{

}