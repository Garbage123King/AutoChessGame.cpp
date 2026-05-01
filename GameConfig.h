#pragma once
#include <vector>
#include <unordered_map>
#include "MonsterData.h"

// 星级属性倍率
struct StarMultiplier {
    float hp;
    float atk;
};

class GameConfig {
public:
    // 获取配置单例
    static const GameConfig& getInstance() {
        static GameConfig instance;
        return instance;
    }

    // 根据怪物ID获取基础模板
    const MonsterData* getMonsterTemplate(const std::string& id) const;
    
    // 获取星级倍率
    StarMultiplier getStarMultiplier(int star) const;
    
    // 获取某等级下的刷新概率 [1费, 2费, 3费, 4费, 5费]
    const std::vector<int>& getLevelOdds(int level) const;
    
    // 获取某费用的卡池数量
    int getDeckCount(int cost) const;
    
    // 获取升级所需经验
    int getLevelXp(int level) const;

    // 暴露给卡池初始化的完整怪物列表
    const std::vector<MonsterData>& getAllMonsters() const { return monsters_; }

private:
    GameConfig(); // 私有构造函数中进行所有硬编码数据的初始化
    ~GameConfig() = default;
    
    // 禁用拷贝
    GameConfig(const GameConfig&) = delete;
    GameConfig& operator=(const GameConfig&) = delete;

    std::vector<MonsterData> monsters_;
    std::unordered_map<int, StarMultiplier> starMultipliers_;
    std::unordered_map<int, std::vector<int>> levelOdds_;
    std::unordered_map<int, int> deckCounts_;
    std::unordered_map<int, int> levelXp_;
};