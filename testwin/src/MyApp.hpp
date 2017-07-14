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

#ifndef MY_APP_HPP_
#define MY_APP_HPP_

//implements the cro::App class

#include <crogine/core/App.hpp>
#include <crogine/core/StateStack.hpp>

#include <crogine/graphics/FontResource.hpp>
#include <crogine/graphics/TextureResource.hpp>

struct SharedResources final
{
    cro::FontResource fonts;
    cro::TextureResource textures;
};
using ResourcePtr = std::unique_ptr<SharedResources>;

class MyApp final : public cro::App
{
public:
	MyApp();
	~MyApp() = default;

private:
	
	cro::StateStack m_stateStack;

	void handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
	void simulate(cro::Time) override;
	void render() override;
    void initialise() override;
    void finalise() override;

    ResourcePtr m_sharedResources;
};

#endif //MY_APP_HPP_