#ifndef NEAT_BOT_H
#define NEAT_BOT_H

#include "game.h"
#include "core/Network.h"
#include <string>

class NeatBot {
public:
    int playerIndex;
    Network net;
    bool loaded;

    NeatBot(int playerIndex, const std::string& genomePath);
    TankActions GetAction(Game* game);
};

#endif
