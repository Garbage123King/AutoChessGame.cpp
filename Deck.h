#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <memory>
#include "MonsterData.h"

class Deck {
public:
    Deck();

    // 刷新商店（返回5个可选怪物的实例，使用智能指针管理内存）
    std::vector<std::shared_ptr<MonsterInstance>> refreshShop(int level);

    // 抽取指定ID的怪物
    std::shared_ptr<MonsterInstance> drawSpecific(const std::string& monsterId);

    // 将怪物放回牌库
    void returnMonster(const std::string& monsterId);

    // 将商店中未购买的怪物放回牌库
    void returnShopMonsters(const std::vector<std::shared_ptr<MonsterInstance>>& shop);

    // 获取当前牌库余量状态 (用于调试或UI展示)
    std::unordered_map<std::string, int> getPoolStatus() const { return pool_; }

private:
    void initPool();
    std::shared_ptr<MonsterInstance> drawOne(int level);
    std::string generateUid(const std::string& monsterId);

    std::unordered_map<std::string, int> pool_;
    std::mt19937 rng_; // 随机数生成器
};