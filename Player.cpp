#include "Player.h"
#include "GameConfig.h"
#include <algorithm>
#include <cmath>

Player::Player(const std::string& id, const std::string& name)
    : id(id), name(name) {
    // 初始化时，强行塞入 9 个空指针，占住茅坑
    bench.resize(MAX_BENCH_SIZE, nullptr);
}

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
    
    // 检查钱够不够
    if (gold < templateData->cost) return false;

    // 检查备战区是否还有空位 (nullptr) ---
    bool hasEmptySlot = false;
    for (const auto& slot : bench) {
        if (slot == nullptr) {
            hasEmptySlot = true;
            break;
        }
    }
    if (!hasEmptySlot) return false; // 备战区满了，不让买！
    
    return true;
}

std::shared_ptr<MonsterInstance> Player::buyMonster(int shopIndex) {
    if (!canBuyMonster(shopIndex)) return nullptr; 
    
    auto monster = shop[shopIndex];
    const auto* templateData = GameConfig::getInstance().getMonsterTemplate(monster->id);
    
    // 寻找第一个【既为空，又没有被封印】的槽位
    int emptySlot = -1;
    for (int i = 0; i < bench.size(); ++i) {
        if (!benchLocked[i] && bench[i] == nullptr) {
            emptySlot = i;
            break;
        }
    }
    
    // 如果虽然有空位，但全被封印了，也不让买
    if (emptySlot == -1) return nullptr; 

    gold -= templateData->cost;
    bench[emptySlot] = monster; 
    shop[shopIndex] = nullptr; 
    
    checkMerge(); 
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
    // 1. 检查越界
    if (benchIndex < 0 || benchIndex >= static_cast<int>(bench.size())) return false;

    if (benchLocked[benchIndex]) return false; // 封印槽位严禁交互

    // --- 2. 新增：检查这个槽位到底有没有怪（不能把空气派上场！） ---
    if (bench[benchIndex] == nullptr) return false;

    // 3. 检查人口上限
    if (static_cast<int>(board.size()) >= level) return false;
    // 4. 检查是否在自己的半场
    if (row < 4 || row > 7) return false;

    // 5. 检查目标位置是否已被占用
    bool occupied = std::any_of(board.begin(), board.end(),
        [&](const auto& m) { return m->row == row && m->col == col; });
    if (occupied) return false;

    // 6. 执行上场逻辑
    auto monster = bench[benchIndex];
    monster->row = row;
    monster->col = col;

    board.push_back(monster);

    // --- 7. 核心修改：绝对不能用 erase！而是把原本的槽位置空 ---
    bench[benchIndex] = nullptr;

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
    
    // 1. 收集棋盘上的怪 (加上判空，以防万一)
    for (const auto& m : board) {
        if (m && m->star == sourceStar) groups[m->id].push_back(m);
    }
    
    // 2. 收集备战区上的怪 (必须判空！因为里面有 nullptr)
    for (const auto& m : bench) {
        if (m && m->star == sourceStar) groups[m->id].push_back(m);
    }

    for (const auto& pair : groups) {
        const auto& units = pair.second;
        if (units.size() >= 3) {
            // 保留第一个怪，升星，并恢复满血等基础状态 (自走棋经典设定)
            auto keep = units[0];
            keep->star = targetStar;
            // 可选：如果是合成升星，一般会把当前血量按比例或者直接回满
            // const auto* tmpl = GameConfig::getInstance().getMonsterTemplate(keep->id);
            // keep->hp = ... 

            // 记录要当成材料吃掉的两个怪的 UID
            std::string removeUid1 = units[1]->uid;
            std::string removeUid2 = units[2]->uid;

            // --- 分离删除逻辑 ---

            // A. 从棋盘中删除（棋盘是动态大小，依然用 erase）
            auto removeFromBoard = [&](const std::string& uid) {
                board.erase(
                    std::remove_if(board.begin(), board.end(), 
                                   [&](const auto& m){ return m && m->uid == uid; }), 
                    board.end()
                );
            };

            // B. 从备战区中清除（备战区是固定槽位，绝对不能 erase，只能置空！）
            auto removeFromBench = [&](const std::string& uid) {
                for (int i = 0; i < bench.size(); ++i) {
                    if (bench[i] && bench[i]->uid == uid) {
                        bench[i] = nullptr; // 留下一个“坑”
                        break;
                    }
                }
            };

            // 挨个执行清理
            removeFromBoard(removeUid1);
            removeFromBench(removeUid1);
            removeFromBoard(removeUid2);
            removeFromBench(removeUid2);

            return true; // 成功合成一次
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