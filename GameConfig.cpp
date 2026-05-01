#include "GameConfig.h"
#include <stdexcept>

GameConfig::GameConfig() {
    // 1. 初始化等级概率表 [1费, 2费, 3费, 4费, 5费]
    levelOdds_ = {
        {1,  {100, 0,  0,  0,  0}},
        {2,  {80,  20, 0,  0,  0}},
        {3,  {70,  25, 5,  0,  0}},
        {4,  {50,  30, 15, 5,  0}},
        {5,  {35,  30, 20, 10, 5}},
        {6,  {20,  30, 25, 15, 10}},
        {7,  {15,  20, 30, 20, 15}},
        {8,  {10,  15, 25, 25, 25}},
        {9,  {5,   10, 20, 30, 35}},
        {10, {0,   5,  15, 30, 50}}
    };

    // 2. 初始化各费用在牌库中的数量
    deckCounts_ = { {1, 9}, {2, 7}, {3, 5}, {4, 3}, {5, 2} };

    // 3. 初始化升级所需经验
    levelXp_ = { {1,0}, {2,2}, {3,6}, {4,10}, {5,20}, {6,32}, {7,46}, {8,64}, {9,86}, {10,110} };

    // 4. 初始化星级倍率
    starMultipliers_ = {
        {1, {1.0f, 1.0f}},
        {2, {1.8f, 1.5f}},
        {3, {3.2f, 2.5f}}
    };

    // 5. 初始化怪物图鉴 (这里演示部分)
    monsters_ = {
        // === 1费怪物 ===
        {
            "selina", "阳光骑士", "赛琳娜·阳曦", 1, 
            550, 30, 0.6f, 1, 5, 5, 80, 0, false, "#FFD700",
            { "圣光打击", "对目标造成魔法伤害并恢复自身生命值", DamageType::Magic, 80, TargetType::Single, 80, 60, 0, 0, 0, 0, false }
        },
        {
            "liana", "风语射手", "莉娅娜·风息", 1, 
            450, 35, 0.7f, 4, 3, 3, 60, 0, true, "#90EE90",
            { "穿透之箭", "射出一支穿透箭，对直线上敌人造成物理伤害", DamageType::Physical, 70, TargetType::Line, 60, 0, 0, 0, 0, 0, false }
        }
        // ... (在这里填入剩余的怪物)
    };
}

const MonsterData* GameConfig::getMonsterTemplate(const std::string& id) const {
    for (const auto& m : monsters_) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

StarMultiplier GameConfig::getStarMultiplier(int star) const {
    if (starMultipliers_.find(star) != starMultipliers_.end()) {
        return starMultipliers_.at(star);
    }
    return starMultipliers_.at(1); // 默认返回1星倍率
}

const std::vector<int>& GameConfig::getLevelOdds(int level) const {
    int safeLevel = std::min(level, 10);
    return levelOdds_.at(safeLevel);
}

int GameConfig::getDeckCount(int cost) const {
    return deckCounts_.count(cost) ? deckCounts_.at(cost) : 0;
}

int GameConfig::getLevelXp(int level) const {
    return levelXp_.count(level) ? levelXp_.at(level) : 9999;
}