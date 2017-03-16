#include "TestApp.hpp"

#include <SDL.h> //needed to properly redfine main() as SDL_main

int main(int argc, char **argv)
{
	//SDL_SetMainReady();

	MyApp app;
	app.run();

	return 0;
}
