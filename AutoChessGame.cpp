#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <random>
#include <algorithm>
#include <memory>

// ==========================================
// 1. 数据模型与配置 (对应 monsters.js)
// ==========================================

struct Skill {
    std::string name;
    std::string description;
    std::string damageType;
    int damage;
    std::string targetType;
    int manaCost;
    int heal = 0;
};

struct MonsterTemplate {
    std::string id;
    std::string name;
    int cost;
    int baseHp;
    int baseAtk;
    double atkSpeed;
    int range;
    int armor;
    int magicResist;
    int maxMana;
    int startMana;
    bool isRanged;
    Skill skill;
};

struct MonsterInstance {
    std::string uid;
    std::string id;
    int star;
    int row;
    int col;
    int hp;
    int maxHp;
    int atk;
    int mana;
    int maxMana;
    int armor;
    int magicResist;
    bool alive;
    
    // 简易构造函数
    MonsterInstance(std::string uid, std::string id, int star = 1) 
        : uid(uid), id(id), star(star), row(-1), col(-1), alive(true) {}
};

// 全局数据单例 (模拟 JS 中的 const 导出)
class GameData {
public:
    static std::vector<MonsterTemplate> MONSTERS;
    static std::map<int, std::vector<int>> LEVEL_ODDS;
    static std::map<int, std::pair<double, double>> STAR_MULTIPLIERS;
    static std::vector<int> LEVEL_XP;
    static std::map<int, int> DECK_COUNTS;
    
    static const MonsterTemplate* getMonster(const std::string& id) {
        for(const auto& m : MONSTERS) {
            if(m.id == id) return &m;
        }
        return nullptr;
    }
};

// 初始化精简版数据
std::vector<MonsterTemplate> GameData::MONSTERS = {
    {"selina", "阳光骑士", 1, 550, 30, 0.6, 1, 5, 5, 80, 0, false, 
     {"圣光打击", "魔法伤害并回血", "magic", 80, "single", 80, 60}},
    {"ignis", "炎狱领主", 5, 1050, 80, 0.65, 3, 20, 25, 120, 20, true, 
     {"地狱之火", "大范围魔法伤害", "magic", 320, "aoe", 120, 0}}
};
std::map<int, std::pair<double, double>> GameData::STAR_MULTIPLIERS = {
    {1, {1.0, 1.0}}, {2, {1.8, 1.5}}, {3, {3.2, 2.25}}
};
std::vector<int> GameData::LEVEL_XP = {0, 0, 2, 4, 8, 14, 24, 36, 50, 70, 100};
std::map<int, int> GameData::DECK_COUNTS = {{1, 29}, {2, 22}, {3, 18}, {4, 12}, {5, 10}};


// ==========================================
// 2. 牌库管理 (对应 deck.js)
// ==========================================

class Deck {
private:
    std::map<std::string, int> pool;
    
    std::string generateUID(const std::string& monsterId) {
        static int counter = 0;
        return monsterId + "_" + std::to_string(++counter);
    }

public:
    Deck() { initPool(); }
    
    void initPool() {
        pool.clear();
        for (const auto& monster : GameData::MONSTERS) {
            pool[monster.id] = GameData::DECK_COUNTS[monster.cost];
        }
    }

    std::shared_ptr<MonsterInstance> drawSpecific(const std::string& monsterId) {
        if (pool[monsterId] <= 0) return nullptr;
        pool[monsterId]--;
        return std::make_shared<MonsterInstance>(generateUID(monsterId), monsterId, 1);
    }

    void returnMonster(const std::string& monsterId) {
        if (pool.find(monsterId) != pool.end()) {
            pool[monsterId]++;
        }
    }

    std::vector<std::shared_ptr<MonsterInstance>> refreshShop(int playerLevel) {
        std::vector<std::shared_ptr<MonsterInstance>> shop;
        // 简化版刷新：随机给5张牌 (原逻辑涉及概率权重，此处用均等随机替代以缩减代码量)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, GameData::MONSTERS.size() - 1);
        
        for (int i = 0; i < 5; i++) {
            auto& tmpl = GameData::MONSTERS[dis(gen)];
            auto m = drawSpecific(tmpl.id);
            if(m) shop.push_back(m);
        }
        return shop;
    }
};


// ==========================================
// 3. 玩家数据 (对应 player.js)
// ==========================================

class Player {
public:
    std::string id;
    std::string name;
    int hp, level, xp, gold, round, streak;
    bool isAlive;
    
    std::vector<std::shared_ptr<MonsterInstance>> board;
    std::vector<std::shared_ptr<MonsterInstance>> bench;
    std::vector<std::shared_ptr<MonsterInstance>> shop;

    Player(std::string id, std::string name) 
        : id(id), name(name), hp(100), level(1), xp(0), gold(10), round(0), streak(0), isAlive(true) {}

    void addXp(int amount) {
        xp += amount;
        while (level < 10 && xp >= GameData::LEVEL_XP[level + 1]) {
            level++;
        }
    }

    bool buyMonster(int shopIndex) {
        if (shopIndex < 0 || shopIndex >= shop.size() || !shop[shopIndex]) return false;
        const auto* tmpl = GameData::getMonster(shop[shopIndex]->id);
        if (!tmpl || gold < tmpl->cost || bench.size() >= 9) return false;
        
        gold -= tmpl->cost;
        bench.push_back(shop[shopIndex]);
        shop.erase(shop.begin() + shopIndex);
        return true;
    }

    bool placeMonster(const std::string& uid, int row, int col) {
        // 简化的上阵逻辑 (不进行三连检查，仅移动)
        if(board.size() >= level) return false; // 人口限制
        
        auto it = std::find_if(bench.begin(), bench.end(), [&](const std::shared_ptr<MonsterInstance>& m){ return m->uid == uid; });
        if (it != bench.end()) {
            (*it)->row = row;
            (*it)->col = col;
            board.push_back(*it);
            bench.erase(it);
            return true;
        }
        return false;
    }
    
    void endRound(bool didWin, int enemySurvived) {
        round++;
        addXp(2);
        if (!didWin && enemySurvived > 0) {
            hp -= enemySurvived * 2;
            if (hp <= 0) {
                hp = 0;
                isAlive = false;
            }
        }
    }
};


// ==========================================
// 4. 战斗引擎 (对应 battle.js)
// ==========================================

class BattleEngine {
public:
    static void initStats(std::shared_ptr<MonsterInstance>& m) {
        const auto* tmpl = GameData::getMonster(m->id);
        if(!tmpl) return;
        auto mult = GameData::STAR_MULTIPLIERS[m->star];
        m->maxHp = std::floor(tmpl->baseHp * mult.first);
        m->hp = m->maxHp;
        m->atk = std::floor(tmpl->baseAtk * mult.second);
        m->mana = tmpl->startMana;
        m->maxMana = tmpl->maxMana;
        m->armor = tmpl->armor + (m->star - 1) * 3;
        m->magicResist = tmpl->magicResist + (m->star - 1) * 5;
        m->alive = true;
    }

    // 返回: {是否玩家获胜, 存活敌人数}
    static std::pair<bool, int> quickSimulate(std::vector<std::shared_ptr<MonsterInstance>>& pBoard, 
                                              std::vector<std::shared_ptr<MonsterInstance>>& eBoard) {
        for(auto& m : pBoard) initStats(m);
        for(auto& m : eBoard) initStats(m);
        
        // 极简回合制战斗演算 (忽略寻路，仅按顺序互相攻击直到一方全灭)
        bool battleOver = false;
        while (!battleOver) {
            int pAlive = 0, eAlive = 0;
            for(auto& m : pBoard) if(m->hp > 0) pAlive++;
            for(auto& m : eBoard) if(m->hp > 0) eAlive++;
            
            if (pAlive == 0 || eAlive == 0) break;

            // 玩家方攻击
            for(auto& pm : pBoard) {
                if(pm->hp <= 0) continue;
                for(auto& em : eBoard) {
                    if(em->hp > 0) {
                        int damage = std::max(1, pm->atk - em->armor); // 简单物理护甲减伤
                        em->hp -= damage;
                        break; // 每次只打一个
                    }
                }
            }
            
            // 敌方攻击
            for(auto& em : eBoard) {
                if(em->hp <= 0) continue;
                for(auto& pm : pBoard) {
                    if(pm->hp > 0) {
                        int damage = std::max(1, em->atk - pm->armor);
                        pm->hp -= damage;
                        break;
                    }
                }
            }
        }
        
        int enemySurvived = 0;
        for(auto& m : eBoard) if(m->hp > 0) enemySurvived++;
        
        return {(enemySurvived == 0), enemySurvived};
    }
};


// ==========================================
// 5. 游戏主控 (对应 gameManager.js)
// ==========================================

class GameManager {
public:
    Deck deck;
    std::shared_ptr<Player> player;
    std::vector<std::shared_ptr<MonsterInstance>> enemyBoard;

    GameManager(std::string playerName) {
        player = std::make_shared<Player>("p1", playerName);
        player->shop = deck.refreshShop(player->level);
        generateEnemy();
    }

    void generateEnemy() {
        enemyBoard.clear();
        int monsterCount = std::max(1, player->level);
        for (int i = 0; i < monsterCount; i++) {
            auto m = deck.drawSpecific("selina"); // 简单起见，敌人全是 1费
            if(m) enemyBoard.push_back(m);
        }
    }

    void runRound() {
        std::cout << "--- 战斗开始 (回合 " << player->round + 1 << ") ---\n";
        
        auto result = BattleEngine::quickSimulate(player->board, enemyBoard);
        bool didWin = result.first;
        int enemySurvived = result.second;
        
        std::cout << "战斗结果: " << (didWin ? "玩家胜利!" : "玩家失败!") << "\n";
        
        player->endRound(didWin, enemySurvived);
        
        // 刷新商店和生成下回合敌人
        player->shop = deck.refreshShop(player->level);
        generateEnemy();
        
        std::cout << "当前HP: " << player->hp << " | 等级: " << player->level << " | 金币: " << player->gold << "\n";
        std::cout << "-----------------------\n";
    }
};

// ==========================================
// 6. 使用示例 (用例)
// ==========================================
int main() {
    std::cout << "========== 正在初始化自走棋游戏 ==========\n";
    
    // 1. 创建游戏房间并命名玩家
    GameManager game("指挥官_Alice");
    auto& p = *game.player;

    std::cout << "玩家 [" << p.name << "] 成功加入游戏。初始金币: " << p.gold << "\n";
    std::cout << ">> 商店刷新了 " << p.shop.size() << " 只怪物。\n";

    // 2. 模拟购买操作
    if (!p.shop.empty()) {
        std::string targetUid = p.shop[0]->uid;
        std::cout << ">> 尝试购买商店第1个怪物...\n";
        if (p.buyMonster(0)) {
            std::cout << "购买成功！剩余金币: " << p.gold << " | 备战区数量: " << p.bench.size() << "\n";
            
            // 3. 将怪物上阵
            std::cout << ">> 将怪物部署到棋盘(行0, 列0)...\n";
            if(p.placeMonster(targetUid, 0, 0)) {
                 std::cout << "部署成功！棋盘怪物数: " << p.board.size() << "\n";
            }
        }
    }

    // 4. 进行回合对战计算
    game.runRound();

    // 5. 模拟第二回合升级并继续买怪
    p.gold += 10; // 模拟发工资
    std::cout << ">> 模拟第二回合，发放金币。当前金币: " << p.gold << "\n";
    
    if (!p.shop.empty()) {
        std::string targetUid2 = p.shop[0]->uid;
        p.buyMonster(0);
        p.placeMonster(targetUid2, 0, 1);
        std::cout << ">> 再次购买并上阵，当前棋盘怪物数: " << p.board.size() << "\n";
    }

    game.runRound();

    std::cout << "========== 游戏演示结束 ==========\n";
    return 0;
}
