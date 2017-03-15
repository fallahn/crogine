#pragma once

#include <crogine/App.hpp>

class MyApp final : public cro::App
{
public:
	MyApp() {}
	~MyApp() {}

private:
	void update(float) override {};
	void handleEvent(const cro::Event&) override {}
	void draw() override {}
};