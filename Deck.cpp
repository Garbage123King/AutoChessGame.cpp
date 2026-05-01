#include "Deck.h"
#include "GameConfig.h"
#include <chrono>

Deck::Deck() {
    // 使用当前时间戳作为随机数种子
    rng_.seed(std::chrono::system_clock::now().time_since_epoch().count());
    initPool();
}

void Deck::initPool() {
    pool_.clear();
    const auto& config = GameConfig::getInstance();
    for (const auto& monster : config.getAllMonsters()) {
        pool_[monster.id] = config.getDeckCount(monster.cost);
    }
}

std::string Deck::generateUid(const std::string& monsterId) {
    // 简单的 UID 生成：ID_时间戳_随机数
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::uniform_int_distribution<int> dist(1000, 9999);
    return monsterId + "_" + std::to_string(now) + "_" + std::to_string(dist(rng_));
}

std::shared_ptr<MonsterInstance> Deck::drawSpecific(const std::string& monsterId) {
    if (pool_[monsterId] <= 0) return nullptr;
    
    pool_[monsterId]--;
    
    auto instance = std::make_shared<MonsterInstance>();
    instance->uid = generateUid(monsterId);
    instance->id = monsterId;
    instance->star = 1;
    
    return instance;
}

std::shared_ptr<MonsterInstance> Deck::drawOne(int level) {
    const auto& config = GameConfig::getInstance();
    const auto& odds = config.getLevelOdds(level);
    
    // 生成 0.0 到 100.0 的随机浮点数
    std::uniform_real_distribution<float> dist(0.0f, 100.0f);
    float roll = dist(rng_);
    
    float cumulative = 0;
    int targetCost = 1;
    for (size_t i = 0; i < odds.size(); i++) {
        cumulative += odds[i];
        if (roll < cumulative) {
            targetCost = static_cast<int>(i) + 1;
            break;
        }
    }

    // 收集符合费用且牌库中有剩余的怪物
    std::vector<const MonsterData*> available;
    for (const auto& m : config.getAllMonsters()) {
        if (m.cost == targetCost && pool_[m.id] > 0) {
            available.push_back(&m);
        }
    }

    // 如果该费用被抽干了，尝试其他所有费用
    if (available.empty()) {
        std::vector<const MonsterData*> allAvailable;
        for (const auto& m : config.getAllMonsters()) {
            if (pool_[m.id] > 0) {
                allAvailable.push_back(&m);
            }
        }
        if (allAvailable.empty()) return nullptr; // 整个牌库都被抽干了
        
        std::uniform_int_distribution<size_t> idxDist(0, allAvailable.size() - 1);
        return drawSpecific(allAvailable[idxDist(rng_)]->id);
    }

    std::uniform_int_distribution<size_t> idxDist(0, available.size() - 1);
    return drawSpecific(available[idxDist(rng_)]->id);
}

void Deck::returnMonster(const std::string& monsterId) {
    if (pool_.find(monsterId) != pool_.end()) {
        pool_[monsterId]++;
    }
}

std::vector<std::shared_ptr<MonsterInstance>> Deck::refreshShop(int level) {
    std::vector<std::shared_ptr<MonsterInstance>> shop;
    for (int i = 0; i < 5; i++) {
        auto monster = drawOne(level);
        // 如果抽到了空（牌库干了），就放个 nullptr 占位
        shop.push_back(monster); 
    }
    return shop;
}

void Deck::returnShopMonsters(const std::vector<std::shared_ptr<MonsterInstance>>& shop) {
    for (const auto& monster : shop) {
        if (monster != nullptr) {
            returnMonster(monster->id);
        }
    }
}