/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "SpriteState.hpp"
#include "SharedStateData.hpp"
#include "GLCheck.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/core/Mouse.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/gui/Gui.hpp>

SpriteState::SpriteState(cro::StateStack& stack, cro::State::Context ctx, SharedStateData& sd)
	: cro::State		(stack, ctx),
	m_sharedData		(sd),
	m_scene				(ctx.appInstance.getMessageBus()),
	m_activeSprite		(nullptr),
	m_showPreferences	(false)
{
	ctx.mainWindow.loadResources([this]()
		{
			initScene();
			createUI();
		});
}

//public
bool SpriteState::handleEvent(const cro::Event& evt)
{
	if (cro::ui::wantsKeyboard()
		|| cro::ui::wantsMouse())
	{
		return false;
	}

	if (evt.type == SDL_MOUSEMOTION)
	{
		if (evt.motion.state & SDL_BUTTON_MIDDLE)
		{
			auto position = m_entities[EntityID::Root].getComponent<cro::Transform>().getPosition();
			position.x += evt.motion.xrel;
			position.y -= evt.motion.yrel;

			position.x = std::floor(position.x);
			position.y = std::floor(position.y);
			m_entities[EntityID::Root].getComponent<cro::Transform>().setPosition(position);
		}
	}
	else if (evt.type == SDL_MOUSEWHEEL)
	{
		auto scale = m_entities[EntityID::Root].getComponent<cro::Transform>().getScale();
		auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
		auto relPos = mousePos - m_entities[EntityID::Root].getComponent<cro::Transform>().getPosition();
		relPos /= scale;

		scale.x = std::max(1.f, std::min(10.f, scale.x + evt.wheel.y));
		scale.y = std::max(1.f, std::min(10.f, scale.y + evt.wheel.y));

		scale.x = std::floor(scale.x);
		scale.y = std::floor(scale.y);

		m_entities[EntityID::Root].getComponent<cro::Transform>().setScale(scale);
		relPos *= scale;
		m_entities[EntityID::Root].getComponent<cro::Transform>().setPosition(mousePos - relPos);
	}
	else if (evt.type == SDL_QUIT)
	{
		if (cro::FileSystem::showMessageBox("Question", "Save Sprite Sheet?", cro::FileSystem::YesNo))
		{
			save();
		}
	}

	m_scene.forwardEvent(evt);
	return false;
}

void SpriteState::handleMessage(const cro::Message& msg)
{
	m_scene.forwardMessage(msg);
}

bool SpriteState::simulate(float dt)
{
	m_scene.simulate(dt);

	return false;
}

void SpriteState::render()
{
	m_scene.render();
}

//private
void SpriteState::initScene()
{
	auto& mb = getContext().appInstance.getMessageBus();

	m_scene.addSystem<cro::SpriteSystem2D>(mb);
	m_scene.addSystem<cro::CameraSystem>(mb);
	m_scene.addSystem<cro::RenderSystem2D>(mb);

	auto resizeCallback = [](cro::Camera& cam)
	{
		auto winSize = glm::vec2(cro::App::getWindow().getSize());
		cam.setOrthographic(0.f, winSize.x, 0.f, winSize.y, -0.1f, 10.f);
		cam.viewport = { 0.f, 0.f, 1.f, 1.f };
	};
	auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
	cam.resizeCallback = resizeCallback;
	resizeCallback(cam);
	m_scene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 3.f });


	m_entities[EntityID::Root] = m_scene.createEntity();
	m_entities[EntityID::Root].addComponent<cro::Transform>();
	m_entities[EntityID::Root].addComponent<cro::Drawable2D>();
	m_entities[EntityID::Root].addComponent<cro::Sprite>();

	m_entities[EntityID::Border] = m_scene.createEntity();
	m_entities[EntityID::Border].addComponent<cro::Transform>().setPosition({0.f, 0.f, 0.1f});
	m_entities[EntityID::Border].addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
	m_entities[EntityID::Root].getComponent<cro::Transform>().addChild(m_entities[EntityID::Border].getComponent<cro::Transform>());

	m_entities[EntityID::Bounds] = m_scene.createEntity();
	m_entities[EntityID::Bounds].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
	m_entities[EntityID::Bounds].addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
	m_entities[EntityID::Root].getComponent<cro::Transform>().addChild(m_entities[EntityID::Bounds].getComponent<cro::Transform>());
}