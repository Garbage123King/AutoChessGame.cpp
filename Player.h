#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "MonsterData.h"
#include "Deck.h"

// 战斗回合结算返回的数据结构
struct GoldResult {
    int income;
    int interest;
    int streak;
    int streakBonus;
};

class Player {
public:
    Player(const std::string& id, const std::string& name);

    // 核心属性获取
    int getXpToLevel() const;
    bool addXp(int amount); // 返回是否升级
    
    // 商店与购买
    bool canRefreshShop() const;
    bool refreshShop(std::shared_ptr<Deck> deck);
    
    bool canLevelUp() const;
    bool buyXp();
    
    bool canBuyMonster(int shopIndex) const;
    std::shared_ptr<MonsterInstance> buyMonster(int shopIndex);
    int sellMonster(const std::string& uid);

    // 棋盘与备战区交互
    bool benchToBoard(int benchIndex, int row, int col);
    bool boardToBench(const std::string& uid);
    bool moveOnBoard(const std::string& uid, int newRow, int newCol);
    bool swapBoardBench(const std::string& boardUid, int benchIndex);

    // 结算
    GoldResult settleGold(bool didWin);
    GoldResult endRound(bool didWin, int enemySurvived);

    // 核心公开状态
    std::string id;
    std::string name;
    int hp = 100;
    int level = 1;
    int xp = 0;
    int gold = 10;
    int round = 0;
    int streak = 0; // 正数为连胜，负数为连败
    bool isAlive = true;

    // 容器管理 (使用智能指针，避免内存泄漏)
    std::vector<std::shared_ptr<MonsterInstance>> board;
    // 把备战区改成固定大小 (例如 9 个槽位)
    static const int MAX_BENCH_SIZE = 9;
    std::vector<std::shared_ptr<MonsterInstance>> bench;
    std::vector<std::shared_ptr<MonsterInstance>> shop;

private:
    void checkMerge();
    bool tryMerge(int targetStar, int sourceStar);
};