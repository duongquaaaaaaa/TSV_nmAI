#include "neat_bot.h"
#include "train/Observation.h"
#include <iostream>

NeatBot::NeatBot(int playerIndex, const std::string& genomePath) : playerIndex(playerIndex), loaded(false) {
    Genome g = Genome::Load(genomePath);
    if (g.nodes.empty()) {
        g = Genome::Load("../" + genomePath);
    }
    
    if (!g.nodes.empty()) {
        net = Network::FromGenome(g);
        loaded = true;
        std::cout << "  [NeatBot] Da tai mo hinh NEAT cho Player " << playerIndex + 1 << " thanh cong!" << std::endl;
    } else {
        std::cerr << "  [NeatBot] Loi: Khong the tai mo hinh NEAT tu " << genomePath << std::endl;
    }
}

TankActions NeatBot::GetAction(Game* game) {
    TankActions action;
    if (!loaded) return action;

    Tank* myTank = nullptr;
    Tank* enemyTank = nullptr;
    for (auto t : game->tanks) {
        if (t->playerIndex == playerIndex) myTank = t;
        else if (!t->isDestroyed) enemyTank = t;
    }

    if (!myTank || myTank->isDestroyed) return action;

    b2Vec2 waypoint = myTank->body->GetPosition();
    if (enemyTank) {
        waypoint = game->map.GetNextWaypoint(myTank->body->GetPosition(), enemyTank->body->GetPosition());
    }

    std::vector<float> obs;
    obs.reserve(36);
    GetObservation(*game, playerIndex, waypoint, obs);

    std::vector<float> agentOut;
    net.Activate(obs, agentOut);

    if (agentOut.size() >= 6) {
        action.forward   = agentOut[0] > 0.5f && agentOut[0] > agentOut[1];
        action.backward  = agentOut[1] > 0.5f && agentOut[1] > agentOut[0];
        action.turnLeft  = agentOut[2] > 0.5f && agentOut[2] > agentOut[3];
        action.turnRight = agentOut[3] > 0.5f && agentOut[3] > agentOut[2];
        action.shoot     = agentOut[4] > 0.5f;
        action.shield    = agentOut[5] > 0.5f;
    }

    return action;
}
