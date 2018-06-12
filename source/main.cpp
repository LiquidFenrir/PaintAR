#include "game.h"

int main(int argc, char* argv[])
{
    consoleDebugInit(debugDevice_SVC);
    auto game = new Game::Game(argc, argv);

    while(aptMainLoop() && game->running)
        game->update();

    delete game;

    return 0;
}
