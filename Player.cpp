#include "Player.h"
#include "GameConfig.h"
#include <algorithm>
#include <cmath>

Player::Player(const std::string& id, const std::string& name)
    : id(id), name(name) {}

int Player::getXpToLevel() const {
    if (level >= 10) return 9999;
    return GameConfig::getInstance().getLevelXp(level + 1) - xp;
}

bool Player::addXp(int amount) {
    xp += amount;
    bool leveledUp = false;
    const auto& config = GameConfig::getInstance();
    
    while (level < 10 && xp >= config.getLevelXp(level + 1)) {
        level++;
        leveledUp = true;
    }
    return leveledUp;
}

bool Player::canRefreshShop() const {
    return gold >= 2;
}

bool Player::refreshShop(std::shared_ptr<Deck> deck) {
    if (!canRefreshShop()) return false;
    
    // 将旧商店的怪物退回卡池
    deck->returnShopMonsters(shop);
    gold -= 2;
    shop = deck->refreshShop(level);
    return true;
}

bool Player::canLevelUp() const {
    return gold >= 4 && level < 10;
}

bool Player::buyXp() {
    if (!canLevelUp()) return false;
    gold -= 4;
    return addXp(4);
}

bool Player::canBuyMonster(int shopIndex) const {
    if (shopIndex < 0 || shopIndex >= static_cast<int>(shop.size())) return false;
    auto monster = shop[shopIndex];
    if (!monster) return false; // 这个格子为空
    
    const auto* templateData = GameConfig::getInstance().getMonsterTemplate(monster->id);
    if (!templateData) return false;
    
    if (gold < templateData->cost) return false;
    if (bench.size() >= 9) return false; // 备战区满
    
    return true;
}

std::shared_ptr<MonsterInstance> Player::buyMonster(int shopIndex) {
    if (!canBuyMonster(shopIndex)) return nullptr;
    
    auto monster = shop[shopIndex];
    const auto* templateData = GameConfig::getInstance().getMonsterTemplate(monster->id);
    
    gold -= templateData->cost;
    bench.push_back(monster); // 加入备战区
    shop[shopIndex] = nullptr; // 商店格子置空
    
    checkMerge(); // 每次购买后检查是否可以合成升星
    
    return monster;
}

int Player::sellMonster(const std::string& uid) {
    // 辅助 Lambda：根据 uid 查找并移除怪物，返回售价
    auto trySellFrom = [&](std::vector<std::shared_ptr<MonsterInstance>>& container) -> int {
        auto it = std::find_if(container.begin(), container.end(), 
            [&](const auto& m) { return m->uid == uid; });
            
        if (it != container.end()) {
            const auto* templateData = GameConfig::getInstance().getMonsterTemplate((*it)->id);
            int sellPrice = templateData ? (templateData->cost * (*it)->star) : 1;
            container.erase(it); // 移出数组，智能指针自动释放内存
            return sellPrice;
        }
        return 0;
    };

    int price = trySellFrom(board);
    if (price > 0) {
        gold += price;
        return price;
    }

    price = trySellFrom(bench);
    if (price > 0) {
        gold += price;
        return price;
    }
    
    return 0; // 没找到
}

bool Player::benchToBoard(int benchIndex, int row, int col) {
    if (benchIndex < 0 || benchIndex >= static_cast<int>(bench.size())) return false;
    if (static_cast<int>(board.size()) >= level) return false; // 棋盘人口上限
    if (row < 4 || row > 7) return false; // 只能放在自己的半场

    // 检查目标位置是否已被占用
    bool occupied = std::any_of(board.begin(), board.end(), 
        [&](const auto& m) { return m->row == row && m->col == col; });
    if (occupied) return false;

    auto monster = bench[benchIndex];
    monster->row = row;
    monster->col = col;
    
    board.push_back(monster);
    bench.erase(bench.begin() + benchIndex);
    return true;
}

bool Player::boardToBench(const std::string& uid) {
    if (bench.size() >= 9) return false;

    auto it = std::find_if(board.begin(), board.end(), 
        [&](const auto& m) { return m->uid == uid; });
        
    if (it == board.end()) return false;

    auto monster = *it;
    monster->row = -1; // 清除坐标
    monster->col = -1;
    
    bench.push_back(monster);
    board.erase(it);
    return true;
}

bool Player::moveOnBoard(const std::string& uid, int newRow, int newCol) {
    if (newRow < 4 || newRow > 7) return false;

    // 找到目标怪
    auto it = std::find_if(board.begin(), board.end(), [&](const auto& m) { return m->uid == uid; });
    if (it == board.end()) return false;

    // 检查新位置是否有人
    bool occupied = std::any_of(board.begin(), board.end(), 
        [&](const auto& m) { return m->uid != uid && m->row == newRow && m->col == newCol; });
    if (occupied) return false;

    (*it)->row = newRow;
    (*it)->col = newCol;
    return true;
}

bool Player::swapBoardBench(const std::string& boardUid, int benchIndex) {
    if (benchIndex < 0 || benchIndex >= static_cast<int>(bench.size())) return false;

    auto boardIt = std::find_if(board.begin(), board.end(), [&](const auto& m) { return m->uid == boardUid; });
    if (boardIt == board.end()) return false;

    auto boardMonster = *boardIt;
    auto benchMonster = bench[benchIndex];

    // 交换坐标
    benchMonster->row = boardMonster->row;
    benchMonster->col = boardMonster->col;
    boardMonster->row = -1;
    boardMonster->col = -1;

    // 交换容器位置
    board.erase(boardIt);
    board.push_back(benchMonster);
    bench.erase(bench.begin() + benchIndex);
    bench.push_back(boardMonster);

    return true;
}

void Player::checkMerge() {
    bool merged = true;
    while (merged) {
        merged = false;
        // 先检查能否合成3星，再检查能否合成2星
        merged = tryMerge(3, 2) || merged;
        merged = tryMerge(2, 1) || merged;
    }
}

bool Player::tryMerge(int targetStar, int sourceStar) {
    std::unordered_map<std::string, std::vector<std::shared_ptr<MonsterInstance>>> groups;
    
    // 收集棋盘和备战区中符合星级的怪
    for (const auto& m : board) if (m->star == sourceStar) groups[m->id].push_back(m);
    for (const auto& m : bench) if (m->star == sourceStar) groups[m->id].push_back(m);

    for (const auto& pair : groups) {
        const auto& units = pair.second;
        if (units.size() >= 3) {
            // 保留第一个怪，升星
            auto keep = units[0];
            keep->star = targetStar;

            // 记录要删除的两个怪的 UID
            std::string removeUid1 = units[1]->uid;
            std::string removeUid2 = units[2]->uid;

            // 闭包函数：在指定容器中删除 UID
            auto removeFunc = [&](std::vector<std::shared_ptr<MonsterInstance>>& container, const std::string& uid) {
                container.erase(
                    std::remove_if(container.begin(), container.end(), 
                                   [&](const auto& m){ return m->uid == uid; }), 
                    container.end()
                );
            };

            // 从棋盘或备战区中将这两个作为材料的怪清除
            removeFunc(board, removeUid1);
            removeFunc(bench, removeUid1);
            removeFunc(board, removeUid2);
            removeFunc(bench, removeUid2);

            return true;
        }
    }
    return false;
}

GoldResult Player::settleGold(bool didWin) {
    int income = 5; // 基础收入
    int interest = std::min(5, gold / 10); // 利息上限为5
    income += interest;

    // 更新连胜/连败记录
    if (didWin) {
        streak = streak > 0 ? streak + 1 : 1;
    } else {
        streak = streak < 0 ? streak - 1 : -1;
    }

    int streakBonus = std::min(3, std::abs(streak) - 1);
    if (streakBonus > 0) income += streakBonus;

    gold += income;
    return { income, interest, streak, streakBonus };
}

GoldResult Player::endRound(bool didWin, int enemySurvived) {
    round++;
    addXp(2); // 每回合自带2经验
    
    if (!didWin && enemySurvived > 0) {
        hp -= enemySurvived * 2;
        if (hp <= 0) {
            hp = 0;
            isAlive = false;
        }
    }
    return settleGold(didWin);
}