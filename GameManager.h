#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Player.h"
#include "Deck.h"
#include "BattleEngine.h"

class GameManager {
public:
    std::vector<std::shared_ptr<MonsterInstance>> upcomingEnemies;

    // 初始化游戏
    GameManager(const std::string& playerName);

    // 玩家基础操作透传
    bool refreshShop();
    bool buyXp();
    bool buyMonster(int shopIndex);
    int sellMonster(const std::string& uid);
    bool benchToBoard(int benchIndex, int row, int col);
    bool boardToBench(const std::string& uid);
    bool moveOnBoard(const std::string& uid, int newRow, int newCol);
    bool swapBoardBench(const std::string& boardUid, int benchIndex);

    // 核心流程控制
    // 开始战斗并返回录像结果
    BattleResult startBattle(bool detailed = true);

    // 获取内部状态 (供 Godot 包装层读取)
    std::shared_ptr<Player> getPlayer() const { return player; }
    const std::vector<std::shared_ptr<MonsterInstance>>& getEnemyBoard() const { return enemyBoard; }
    std::string getPhase() const { return phase; }
    int getRound() const { return round; }

    void prepareNextWave(int round); // 准备下一波敌人
    bool useSeerTent();              // 预言屋接口

    // 黑市接口
    void signBlackMarketContract();  // 签订高利贷契约
    bool payBlackMarketDebt();       // 尝试还钱
    void processRoundEnd();          // 回合结算（扣除还款期限）

private:
    void generateEnemy(); // 生成当前回合的敌方 AI

    std::shared_ptr<Player> player;
    std::shared_ptr<Deck> deck;
    std::vector<std::shared_ptr<MonsterInstance>> enemyBoard;
    
    std::string phase; // "prepare" | "battle"
    int round;
};