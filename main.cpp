#include <raylib.h>
#include <ctime>
#include <cstdlib>
#include <string>
#include <iostream>
#include <algorithm>
#include "game.h"
#include "renderer.h"
#include "ui.h"
#include "bot.h"
#include "ai_bot.h"
#include "Astar/astar_bot.h"
#include "core/Network.h"
#include "train/Observation.h"
#include "train/Curriculum.h"
#include "train/RuleEnemy.h"

#ifdef _WIN32
// Khai báo trực tiếp 2 hàm Win32 cần dùng thay vì #include <windows.h>
// để tránh xung đột tên (Rectangle, CloseWindow, ShowCursor) với Raylib.
extern "C" {
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
    __declspec(dllimport) int __stdcall SetCurrentDirectoryA(const char* lpPathName);
}
#define MAX_PATH 260
#endif

/**
 * @brief Entry point cho chế độ human play (có đồ họa).
 * 
 * Kiến trúc:
 * - Game: quản lý logic thuần (Box2D), KHÔNG phụ thuộc Raylib
 * - Renderer: vẽ thế giới game
 * - UI: giao diện cài đặt + HUD
 * - main.cpp: kết nối input (bàn phím → TankActions) và rendering
 * 
 * Chạy mặc định để chơi game thường (Người vs Người/Bot/A-star/RL).
 * Truyền thêm `--watch <não_ai>` để theo dõi AI NEAT tự chơi.
 */
int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));

    bool watchMode = false;
    Network agentNet, enemyNet;
    bool hasEnemyNet = false;
    std::string brainPath = "";
    int watchFrameCounter = 0;
    Phase watchPhase = Phase::PHASE1;
    EnemyType watchEnemyOverride = EnemyType::STATIONARY;
    bool hasEnemyOverride = false;
    const int WATCH_MAX_FRAMES = 60 * 15; // Giới hạn 15 giây (60 FPS) cho mỗi ván khi rảnh

    if (argc >= 3 && std::string(argv[1]) == "--watch") {
        watchMode = true;
        brainPath = argv[2];
        Genome g = Genome::Load(brainPath);
        if (g.nodes.empty()) {
            std::cerr << "Failed to load brain: " << brainPath << std::endl;
            return 1;
        }
        std::string pathLower = brainPath;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        if (pathLower.find("phase2") != std::string::npos) watchPhase = Phase::PHASE2;
        else if (pathLower.find("phase3") != std::string::npos) watchPhase = Phase::PHASE3;
        else if (pathLower.find("phase4") != std::string::npos) watchPhase = Phase::PHASE4;
        else if (pathLower.find("phase5") != std::string::npos) watchPhase = Phase::PHASE5;

        agentNet = Network::FromGenome(g);
        std::cout << "Loaded AI brain: " << brainPath << std::endl;

        if (argc >= 4) {
            std::string enemySpec = argv[3];
            std::string specLower = enemySpec;
            std::transform(specLower.begin(), specLower.end(), specLower.begin(), ::tolower);

            if (specLower == "rule_v1") { watchEnemyOverride = EnemyType::RULE_V1; hasEnemyOverride = true; }
            else if (specLower == "rule_v2") { watchEnemyOverride = EnemyType::RULE_V2; hasEnemyOverride = true; }
            else if (specLower == "rule_v3") { watchEnemyOverride = EnemyType::RULE_V3; hasEnemyOverride = true; }
            else if (specLower == "random") { watchEnemyOverride = EnemyType::RANDOM; hasEnemyOverride = true; }
            else if (specLower == "stationary") { watchEnemyOverride = EnemyType::STATIONARY; hasEnemyOverride = true; }
            else {
                // Thử load như là một file não .bin
                Genome eg = Genome::Load(enemySpec);
                if (!eg.nodes.empty()) {
                    enemyNet = Network::FromGenome(eg);
                    hasEnemyNet = true;
                    std::cout << "Loaded Enemy brain (SELF-PLAY): " << enemySpec << std::endl;
                }
            }
        }

        if (argc >= 5) {
            int p = std::atoi(argv[4]);
            if (p >= 1 && p <= 5) {
                watchPhase = static_cast<Phase>(p);
                std::cout << "Forced Environment Phase: " << p << std::endl;
            }
        }
    }

    // ================================================================
    // Tự động chuyển CWD về thư mục chứa file exe.
    // ================================================================
#ifdef _WIN32
    {
        char exePath[MAX_PATH];
        unsigned long len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            char* lastSlash = strrchr(exePath, '\\');
            if (lastSlash) {
                *lastSlash = '\0';
                SetCurrentDirectoryA(exePath);
            }
        }
    }
#endif

    Game game;
    // Cài đặt phím mặc định cho 4 người chơi (Raylib key codes)
    game.configs[0] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q, KEY_E};
    game.configs[1] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SLASH, KEY_PERIOD};
    game.configs[2] = {KEY_I, KEY_K, KEY_J, KEY_L, KEY_U, KEY_O};
    game.configs[3] = {KEY_KP_8, KEY_KP_5, KEY_KP_4, KEY_KP_6, KEY_KP_7, KEY_KP_9};

    std::vector<bool> isBot   = {false, true, false, false}; // P1 là người, P2 mặc định là Bot
    std::vector<bool> isAI    = {false, false, false, false}; // Không ai là AI (neural net) mặc định
    std::vector<bool> isAstar = {false, false, false, false}; // A* pathfinding agent mặc định

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game");
    SetTargetFPS(60);
    UI::Init();

    // Khởi tạo các Bot bên ngoài vòng lặp chính để chúng không bị "mất trí nhớ" mỗi frame
    std::vector<Bot*>       bots(4, nullptr);
    std::vector<AIBot*>     aiBots(4, nullptr);
    std::vector<AStarBot*>  astarBots(4, nullptr);
    if (!watchMode && isBot[1] && !isAI[1] && !isAstar[1]) bots[1] = new Bot(7, 1);

    while (!WindowShouldClose()) {
        // --- Xử lý Settings UI ---
        if (!watchMode && UI::CheckSettingsButtonClicked()) {
            int oldNumPlayers = game.numPlayers;
            UI::ShowSettingsScreen(game.numPlayers, game.portalsEnabled, game.itemsEnabled, game.shieldsEnabled, game.configs, isBot, isAI, isAstar);
            if (game.numPlayers != oldNumPlayers) {
                for (int i = 0; i < 4; i++) game.playerScores[i] = 0;
            }
            // Cập nhật lại danh sách bot nếu có thay đổi trong cài đặt
            for (int i = 0; i < 4; i++) {
                auto clearAll = [&]() {
                    if (bots[i])      { delete bots[i];      bots[i]      = nullptr; }
                    if (aiBots[i])   { delete aiBots[i];   aiBots[i]   = nullptr; }
                    if (astarBots[i]){ delete astarBots[i]; astarBots[i] = nullptr; }
                };
                if (isBot[i] && !isAI[i] && !isAstar[i]) {
                    clearAll();
                    bots[i] = new Bot(7, i);          // Rule-based Bot
                } else if (isBot[i] && isAstar[i]) {
                    clearAll();
                    astarBots[i] = new AStarBot(i);   // A* Demo Bot
                } else if (isBot[i] && isAI[i]) {
                    clearAll();
                    aiBots[i] = new AIBot(i);          // Neural Network Bot
                } else {
                    clearAll();                        // Người chơi
                }
            }
            game.needsRestart = true;
        }

        if (game.needsRestart || (watchMode && watchFrameCounter >= WATCH_MAX_FRAMES)) {
            if (watchMode) {
                game.numPlayers = 2;
                PhaseConfig cfg = GetPhaseConfig(watchPhase);
                game.mapMode = cfg.mapMode;
                game.bulletLifespan = cfg.bulletLifespan;
                game.maxBullets = cfg.maxBullets;
                game.itemsEnabled = cfg.itemsEnabled;
                game.shieldsEnabled = cfg.shieldsEnabled;
                game.portalsEnabled = cfg.portalsEnabled;
                watchFrameCounter = 0; 
            }
            game.ResetMatch();

            if (!watchMode) {
                // Reset trạng thái bot khi bắt đầu trận mới
                for (int i = 0; i < 4; i++) {
                    if (bots[i]) {
                        bots[i]->requestPathClear = true;
                        bots[i]->lastEnemyPos = b2Vec2(0, 0);
                        bots[i]->stuckCounter = 0;
                        bots[i]->idleCounter  = 0;
                    }
                    if (astarBots[i]) {
                        astarBots[i]->requestPathClear = true;
                        astarBots[i]->lastEnemyPos = b2Vec2(0, 0);
                        astarBots[i]->stuckCounter = 0;
                        astarBots[i]->cachedPath.clear();
                        astarBots[i]->waypointIdx = 0;
                    }
                }
            }
        }

        if (watchMode) watchFrameCounter++;

        // --- Thu thập hành động ---
        std::vector<TankActions> actions(game.numPlayers);
        std::vector<float> agentOut;
        agentOut.reserve(6);
        
        static int astarCounter = 0;
        static b2Vec2 astarWaypoint = {0, 0};
        
        for (int i = 0; i < game.numPlayers; i++) {
            if (watchMode && i == 0) {
                // Recompute A* waypoint every 3 frames (Đồng bộ với train.cpp)
                if (astarCounter <= 0 && game.tanks.size() >= 2) {
                    astarWaypoint = game.map.GetNextWaypoint(
                        game.tanks[0]->body->GetPosition(),
                        game.tanks[1]->body->GetPosition()
                    );
                    astarCounter = 3;
                }
                astarCounter--;
                
                // AI Agent controls Player 0
                std::vector<float> obs;
                obs.reserve(36);
                GetObservation(game, 0, astarWaypoint, obs);
                agentOut.clear();
                agentNet.Activate(obs, agentOut);
                if (agentOut.size() >= 6) {
                    actions[i].forward   = agentOut[0] > 0.5f && agentOut[0] > agentOut[1];
                    actions[i].backward  = agentOut[1] > 0.5f && agentOut[1] > agentOut[0];
                    actions[i].turnLeft  = agentOut[2] > 0.5f && agentOut[2] > agentOut[3];
                    actions[i].turnRight = agentOut[3] > 0.5f && agentOut[3] > agentOut[2];
                    actions[i].shoot     = agentOut[4] > 0.5f;
                    actions[i].shield    = agentOut[5] > 0.5f;

                    if (watchFrameCounter % 60 == 0) {
                        printf("AI: Fwd:%d Bwd:%d L:%d R:%d Sht:%d Shd:%d\n", 
                            actions[i].forward, actions[i].backward, actions[i].turnLeft, actions[i].turnRight, actions[i].shoot, actions[i].shield);
                    }
                }
            } else if (watchMode && i == 1) {
                PhaseConfig cfg = GetPhaseConfig(watchPhase);
                
                if (hasEnemyNet) {
                    // [BỘ NÃO THỨ 2]: Tự đấu với chính mình (Self-Play)
                    std::vector<float> enemyObs(36, 0.0f);
                    b2Vec2 enemyWp = game.map.GetNextWaypoint(game.tanks[1]->body->GetPosition(), game.tanks[0]->body->GetPosition());
                    GetObservation(game, 1, enemyWp, enemyObs);
                    
                    std::vector<float> enemyOut;
                    enemyNet.Activate(enemyObs, enemyOut);
                    if (enemyOut.size() >= 6) {
                        actions[i].forward   = enemyOut[0] > 0.5f && enemyOut[0] > enemyOut[1];
                        actions[i].backward  = enemyOut[1] > 0.5f && enemyOut[1] > enemyOut[0];
                        actions[i].turnLeft  = enemyOut[2] > 0.5f && enemyOut[2] > enemyOut[3];
                        actions[i].turnRight = enemyOut[3] > 0.5f && enemyOut[3] > enemyOut[2];
                        actions[i].shoot     = enemyOut[4] > 0.5f;
                        actions[i].shield    = enemyOut[5] > 0.5f;
                    }
                } else {
                    EnemyType eType = hasEnemyOverride ? watchEnemyOverride : cfg.enemyType;

                    if (eType == EnemyType::STATIONARY) {
                        actions[i] = {}; // Đứng im
                    } else {
                        RuleVariant rv = RuleVariant::V1;
                        if (eType == EnemyType::RULE_V2) rv = RuleVariant::V2;
                        if (eType == EnemyType::RULE_V3) rv = RuleVariant::V3;
                        
                        if (eType == EnemyType::RANDOM) {
                            actions[i].forward = (rand() % 3 == 0);
                            actions[i].turnLeft = (rand() % 4 == 0);
                            actions[i].turnRight = (rand() % 4 == 0);
                            actions[i].shoot = (rand() % 90 == 0);
                            actions[i].shield = (rand() % 150 == 0);
                        } else {
                            actions[i] = GetRuleEnemyAction(game, 1, rv);
                            if (rv != RuleVariant::V3 && rand() % 100 < 50) actions[i].shoot = false; 
                        }
                    }
                }
            } else {
                // Chế độ chơi thường: gán phím hoặc Bot AI
                if (isBot[i] && isAI[i] && aiBots[i]) {
                    actions[i] = aiBots[i]->GetAction(&game);
                } else if (isBot[i] && isAstar[i] && astarBots[i]) {
                    actions[i] = astarBots[i]->GetAction(&game);
                } else if (isBot[i] && bots[i]) {
                    actions[i] = bots[i]->GetAction(&game);
                } else {
                    PlayerConfig& cfg = game.configs[i];
                    actions[i].forward = IsKeyDown(cfg.fw);
                    actions[i].backward = IsKeyDown(cfg.bw);
                    actions[i].turnLeft = IsKeyDown(cfg.tl);
                    actions[i].turnRight = IsKeyDown(cfg.tr);
                    actions[i].shoot = IsKeyPressed(cfg.sh);
                    actions[i].shield = IsKeyPressed(cfg.shieldKey);
                }
            }
        }

        // --- Cập nhật logic game ---
        float dt = GetFrameTime();
        game.Update(actions, dt);

        // --- Cập nhật hiệu ứng đồ họa ---
        Renderer::Update(game, dt);

        // --- Render ---
        BeginDrawing();
        ClearBackground({245, 240, 230, 255}); // Nền beige ấm
        Renderer::DrawWorld(game);

        // [PHÍM TẮT V] - Bật/Tắt hiển thị Waypoint của AI (chỉ dùng trong watch mode)
        static bool showWaypoint = true;
        if (IsKeyPressed(KEY_V)) showWaypoint = !showWaypoint;

        if (watchMode && showWaypoint && game.tanks.size() >= 2) {
            float pixelX = astarWaypoint.x * 30.0f; 
            float pixelY = SCREEN_HEIGHT - (astarWaypoint.y * 30.0f);
            DrawCircle((int)pixelX, (int)pixelY, 5.0f, RED);
            DrawLineEx(
                {game.tanks[0]->body->GetPosition().x * 30.0f, SCREEN_HEIGHT - (game.tanks[0]->body->GetPosition().y * 30.0f)},
                {pixelX, pixelY},
                2.0f, Fade(RED, 0.4f)
            );
            DrawText("WAYPOINT (V to toggle)", (int)pixelX + 10, (int)pixelY - 10, 10, DARKGRAY);
        }

        // Vẽ đường đi bot A* (chỉ dùng trong chế độ chơi thường)
        if (!watchMode) {
            Renderer::DrawBotPaths(game);
        }

        UI::DrawHUD(game.playerScores, game.numPlayers);
        EndDrawing();
    }

    // Dọn dẹp bộ nhớ
    for (int i = 0; i < 4; i++) {
        if (bots[i])      delete bots[i];
        if (aiBots[i])   delete aiBots[i];
        if (astarBots[i]) delete astarBots[i];
    }
    UI::Cleanup();
    CloseWindow();
    return 0;
}