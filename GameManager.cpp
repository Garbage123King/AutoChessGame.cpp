#include "GameManager.h"
#include "GameConfig.h"
#include <random>
#include <chrono>
#include <algorithm>

GameManager::GameManager(const std::string& playerName) {
    player = std::make_shared<Player>("p1_id", playerName);
    deck = std::make_shared<Deck>();
    
    phase = "prepare";
    round = 1;
    
    // 初始化第一回合的商店和敌人
    player->shop = deck->refreshShop(player->level);
    generateEnemy();
}

void GameManager::generateEnemy() {
    enemyBoard.clear();
    int enemyLevel = std::max(1, player->level);
    int monsterCount = enemyLevel; // 简单的 AI：等级等于怪物数
    
    const auto& config = GameConfig::getInstance();
    const auto& odds = config.getLevelOdds(enemyLevel);
    const auto& allMonsters = config.getAllMonsters();

    std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<float> dist(0.0f, 100.0f);

    for (int i = 0; i < monsterCount; i++) {
        float roll = dist(rng);
        float cumulative = 0;
        int targetCost = 1;
        
        for (size_t j = 0; j < odds.size(); j++) {
            cumulative += odds[j];
            if (roll < cumulative) {
                targetCost = static_cast<int>(j) + 1;
                break;
            }
        }

        std::vector<const MonsterData*> candidates;
        for (const auto& m : allMonsters) {
            if (m.cost == targetCost) candidates.push_back(&m);
        }

        if (!candidates.empty()) {
            std::uniform_int_distribution<size_t> idxDist(0, candidates.size() - 1);
            const auto* chosen = candidates[idxDist(rng)];

            auto enemy = std::make_shared<MonsterInstance>();
            enemy->uid = "enemy_" + chosen->id + "_" + std::to_string(i);
            enemy->id = chosen->id;
            
            // 后期有概率刷出2星敌人
            std::uniform_real_distribution<float> starDist(0.0f, 1.0f);
            enemy->star = (player->level >= 6 && starDist(rng) < 0.3f) ? 2 : 1;
            
            // 随机放在上半区 (0-3行)
            std::uniform_int_distribution<int> rowDist(0, 3);
            std::uniform_int_distribution<int> colDist(0, 7);
            
            // 简易查重（避免坐标重叠）
            bool occupied = true;
            while(occupied) {
                enemy->row = rowDist(rng);
                enemy->col = colDist(rng);
                occupied = std::any_of(enemyBoard.begin(), enemyBoard.end(), [&](const auto& m){
                    return m->row == enemy->row && m->col == enemy->col;
                });
            }
            
            enemyBoard.push_back(enemy);
        }
    }
}

// ==================== GameManager.cpp 补充 ====================

bool GameManager::refreshShop() {
    return player->refreshShop(deck);
}

bool GameManager::buyXp() {
    return player->buyXp();
}

bool GameManager::buyMonster(int shopIndex) {
    // player->buyMonster 返回的是智能指针，转为 bool 表示是否购买成功
    return player->buyMonster(shopIndex) != nullptr;
}

int GameManager::sellMonster(const std::string& uid) {
    return player->sellMonster(uid);
}

bool GameManager::benchToBoard(int benchIndex, int row, int col) {
    return player->benchToBoard(benchIndex, row, col);
}

bool GameManager::boardToBench(const std::string& uid) {
    return player->boardToBench(uid);
}

bool GameManager::moveOnBoard(const std::string& uid, int newRow, int newCol) {
    return player->moveOnBoard(uid, newRow, newCol);
}

bool GameManager::swapBoardBench(const std::string& boardUid, int benchIndex) {
    return player->swapBoardBench(boardUid, benchIndex);
}

BattleResult GameManager::startBattle(bool detailed) {
    if (phase != "prepare" || player->board.empty()) {
        return BattleResult{}; // 状态不对或棋盘没怪，返回空
    }

    phase = "battle";
    BattleEngine engine;
    BattleResult result;

    if (detailed) {
        result = engine.simulate(player->board, enemyBoard);
    } else {
        result = engine.quickSimulate(player->board, enemyBoard);
    }

    bool didWin = (result.winner == "player");
    player->endRound(didWin, result.enemySurvived);

    // 准备下一回合
    round++;
    generateEnemy();
    deck->returnShopMonsters(player->shop);
    player->shop = deck->refreshShop(player->level);
    phase = "prepare";

    return result;
}