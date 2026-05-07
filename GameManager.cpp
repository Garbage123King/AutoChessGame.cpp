#include "GameManager.h"
#include "GameConfig.h"
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>

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

// 提前生成下一波敌人（在每一回合开始时，或者上一回合结束时调用）
void GameManager::prepareNextWave(int round) {
    upcomingEnemies.clear();

    // 这里是你原本生成敌人的逻辑，比如：
    // upcomingEnemies.push_back(createMonster("goblin", 1));
    // upcomingEnemies.push_back(createMonster("orc", 1));

    // --- 💣 讨债人降临判定 ---
    // 如果欠钱没还，且期限已到，直接在敌方阵容里强行塞入讨债人！
    if (player->debt > 0 && player->debtRoundsLeft <= 0) {
        auto repoMan = std::make_shared<MonsterInstance>();
        repoMan->uid = "REPO_MAN_BOSS";
        repoMan->id = "black_market_thug"; // 需要你在 GameConfig 里配一个逆天的怪
        repoMan->star = 3;
        repoMan->row = 4; // 放在敌方最前排中心
        repoMan->col = 4;
        upcomingEnemies.push_back(repoMan);
        std::cout << "\n⚠️ 警告：期限已至！黑市讨债人已加入敌方阵营！⚠️\n";
    }
}

// 预言屋：花 1 块钱偷看情报
bool GameManager::useSeerTent() {
    if (player->gold < 1) {
        std::cout << "预言屋：你的金币不足，预言家闭上了眼睛。\n";
        return false;
    }

    player->gold -= 1;
    std::cout << "\n🔮 --- 预言屋的启示 --- 🔮\n";
    std::cout << "下一波你将面对 " << upcomingEnemies.size() << " 名敌人：\n";
    for (const auto& enemy : upcomingEnemies) {
        std::cout << " - [" << enemy->star << "星] " << enemy->id << "\n";
    }
    std::cout << "-----------------------\n";
    return true;
}

// 签订高利贷契约（仅限开局或城镇阶段调用）
void GameManager::signBlackMarketContract() {
    if (player->debt > 0) {
        std::cout << "黑市商人：你上次的账还没清呢，滚出去！\n";
        return;
    }

    std::cout << "\n🩸 你签订了鲜血契约...\n";

    // 1. 获得 20 金币
    player->gold += 20;

    // 2. 欠下 40 金币，限期 5 回合
    player->debt = 40;
    player->debtRoundsLeft = 5;

    // 3. 永久扣除 30 点生命值（直接扣当前和上限）
    player->hp -= 30;
    if (player->hp <= 0) player->hp = 1; // 留 1 滴血，别当场死了

    // 4. 封印备战区最后 2 个槽位 (索引 7 和 8)
    // 注意：假设这是开局调用的，这俩槽位肯定是空的。如果是中途调用，最好把里面的怪卖掉或挤走。
    player->benchLocked[7] = true;
    player->benchLocked[8] = true;

    std::cout << "获得了 20 金币！你的最大生命值已被削弱，备战区 2 个槽位被死灵魔法封印。\n";
    std::cout << "请在 5 回合内还清 40 金币，否则...\n";
}

// 尝试还钱
bool GameManager::payBlackMarketDebt() {
    if (player->debt <= 0) {
        std::cout << "黑市商人：你不欠我们钱。\n";
        return false;
    }

    if (player->gold < player->debt) {
        std::cout << "黑市商人：钱不够？攒够 40 块再来找我！\n";
        return false;
    }

    // 还钱！
    player->gold -= player->debt;
    player->debt = 0;

    // 解除封印！
    player->benchLocked[7] = false;
    player->benchLocked[8] = false;

    std::cout << "\n💰 你还清了黑市的 40 金币债务！\n";
    std::cout << "封印解除，备战区恢复正常，讨债人取消了对你的追杀计划。\n";
    // 注意：失去的 30 点生命值不会回来，这是代价！

    return true;
}

// 每回合结束时调用，倒计时逼近
void GameManager::processRoundEnd() {
    if (player->debt > 0) {
        player->debtRoundsLeft--;
        if (player->debtRoundsLeft > 0) {
            std::cout << "🔔 黑市提醒：距离最后还款期限还有 " << player->debtRoundsLeft << " 回合。\n";
        }
        else if (player->debtRoundsLeft == 0) {
            std::cout << "💀 黑市最后通牒：时间已到，讨债人已上路。\n";
        }
    }
}