#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "MonsterData.h"

// 设定引擎物理帧率
constexpr int TPS = 30;

// 坐标使用浮点数，支持平滑移动和弹道飞行
struct Position {
    float row;
    float col;
};

// 单位当前状态
enum class UnitState {
    Idle,       // 寻敌中
    Moving,     // 移动中
    Windup,     // 攻击/施法前摇（举起武器）
    Backswing,  // 攻击/施法后摇（收起武器，冷却中）
    Stunned     // 眩晕中
};

// 战斗过程中的动作类型枚举
enum class ActionType {
    Move, Attack, Cast, Heal, Shield, Aoe, Line, SkillDamage, Stun, Slow, 
    Windup, SpawnProjectile, ProjectileHit
};

// 单个动作记录（例如：A 移动到了 X,Y；A 对 B 造成了 100 伤害）
struct BattleAction {
    ActionType type;
    std::string uid;             // 动作发起者
    std::string targetUid = "";  // 动作目标（如果有）
    
    // 可选参数 (C++17 std::optional 非常适合处理这些按需存在的参数)
    std::optional<int> damage;
    std::optional<DamageType> damageType;
    std::optional<int> amount;   // 治疗量/护盾量/持续时间
    std::optional<Position> from;
    std::optional<Position> to;
    std::optional<int> radius;
    std::string skillName = "";
};

// 战斗单位状态快照 (记录每一帧怪物的位置和血量，用于 Godot 差值平滑动画)
struct UnitSnapshot {
    std::string uid;
    int team; // 0: 玩家, 1: 敌人
    float row;
    float col;
    int hp, maxHp, mana, maxMana, shield;
    bool alive;
    int stunTimer;
};

// 每一帧的数据
struct Tick {
    int frame;
    std::vector<BattleAction> actions;
    std::vector<UnitSnapshot> units;
};

// 最终战斗结果
struct BattleResult {
    std::string winner; // "player", "enemy", "draw"
    int playerSurvived;
    int enemySurvived;
    std::vector<Tick> ticks; // 详细的录像数据 (快速模拟时可为空)
};

struct Projectile {
    std::string uid;
    std::string sourceUid;
    std::string targetUid;
    Position currentPos;
    Position targetPos; // 目标死亡时飞向最后已知位置
    float speed;        // 每帧飞行的距离 (格/Tick)
    int damage;
    DamageType damageType;
    bool hit = false;   // 是否已命中销毁
};

// 内部战斗单位实体 (包含所有易变状态)
struct BattleUnit {
    std::string uid;
    std::string id;
    int team;
    int star;
    
    // 位置，支持移动
    float row;
    float col;

    int hp, maxHp;
    int atk, mana, maxMana, armor, magicResist;
    int range;
    float atkSpeed;
    bool isRanged;
    SkillData skill;
    bool alive = true;

    // --- 状态机专用参数 ---
    UnitState state = UnitState::Idle;
    int stateTicks = 0;             // 当前状态剩余的 Tick 数
    std::string currentTargetUid;   // 当前锁定的目标
    Position nextMovePos;           // 移动的下一个格子

    int attackCooldown = 0;
    int stunTimer = 0;
    int slowTimer = 0;
    int shield = 0;
};

class BattleEngine {
public:
    BattleEngine() : boardRows(8), boardCols(8) {}

    // 详细模拟（返回完整 Ticks 供 Godot 播放）
    BattleResult simulate(const std::vector<std::shared_ptr<MonsterInstance>>& playerUnits, 
                          const std::vector<std::shared_ptr<MonsterInstance>>& enemyUnits);

    // 快速模拟（直接跳过录像，只出结果，用于跳过战斗功能）
    BattleResult quickSimulate(const std::vector<std::shared_ptr<MonsterInstance>>& playerUnits, 
                               const std::vector<std::shared_ptr<MonsterInstance>>& enemyUnits);

private:
    int boardRows;
    int boardCols;
    std::vector<Projectile> activeProjectiles;

    BattleUnit* findUnitByUid(const std::string& uid, const std::vector<std::unique_ptr<BattleUnit>>& allUnits);
    std::optional<Position> calculateNextPath(BattleUnit* unit, BattleUnit* target, const std::vector<std::unique_ptr<BattleUnit>>& allUnits);

    std::unique_ptr<BattleUnit> createBattleUnit(const std::shared_ptr<MonsterInstance>& instance, int team);
    
    int calcDamage(int baseDamage, DamageType type, int armor, int magicResist) const;
    void applyDamage(BattleUnit* unit, int damage);
    float distance(const BattleUnit* a, const BattleUnit* b) const;
    
    BattleUnit* findClosestEnemy(BattleUnit* unit, const std::vector<std::unique_ptr<BattleUnit>>& allUnits);
    std::optional<Position> moveToward(BattleUnit* unit, BattleUnit* target, const std::vector<std::unique_ptr<BattleUnit>>& allUnits, float speed);
    
    void castSkill(BattleUnit* caster, const std::vector<std::unique_ptr<BattleUnit>>& allUnits, std::vector<BattleAction>& actionsOut);
};