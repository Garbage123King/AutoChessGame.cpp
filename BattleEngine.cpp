#include "BattleEngine.h"
#include "GameConfig.h"
#include <cmath>
#include <algorithm>

// --- 辅助数学与公式 ---

float BattleEngine::distance(const BattleUnit* a, const BattleUnit* b) const {
    return std::sqrt(std::pow(a->row - b->row, 2) + std::pow(a->col - b->col, 2));
}

int BattleEngine::calcDamage(int baseDamage, DamageType type, int armor, int magicResist) const {
    float reduction = 0.0f;
    if (type == DamageType::Physical) {
        // 物理伤害减免公式
        reduction = static_cast<float>(armor) / (armor + 100.0f);
    } else {
        // 魔法伤害减免公式
        reduction = static_cast<float>(magicResist) / (magicResist + 100.0f);
    }
    int finalDamage = static_cast<int>(baseDamage * (1.0f - reduction));
    return std::max(1, finalDamage); // 最低造成1点伤害
}

void BattleEngine::applyDamage(BattleUnit* unit, int damage) {
    if (unit->shield > 0) {
        if (unit->shield >= damage) {
            unit->shield -= damage;
            return;
        } else {
            damage -= unit->shield;
            unit->shield = 0;
        }
    }
    unit->hp -= damage;
    if (unit->hp <= 0) {
        unit->hp = 0;
        unit->alive = false;
    }
}

// --- 初始化战斗实体 ---

std::unique_ptr<BattleUnit> BattleEngine::createBattleUnit(const std::shared_ptr<MonsterInstance>& instance, int team) {
    const auto* tmpl = GameConfig::getInstance().getMonsterTemplate(instance->id);
    if (!tmpl) return nullptr;

    auto unit = std::make_unique<BattleUnit>();
    auto mult = GameConfig::getInstance().getStarMultiplier(instance->star);

    unit->uid = instance->uid;
    unit->id = instance->id;
    unit->team = team;
    unit->star = instance->star;
    unit->row = instance->row;
    unit->col = instance->col;

    // 属性装配
    unit->hp = unit->maxHp = static_cast<int>(tmpl->baseHp * mult.hp);
    unit->atk = static_cast<int>(tmpl->baseAtk * mult.atk);
    unit->mana = tmpl->startMana;
    unit->maxMana = tmpl->maxMana;
    // 星级带来的额外护甲魔抗
    unit->armor = tmpl->armor + (instance->star - 1) * 3;
    unit->magicResist = tmpl->magicResist + (instance->star - 1) * 5;
    
    unit->range = tmpl->range;
    unit->atkSpeed = tmpl->atkSpeed;
    unit->isRanged = tmpl->isRanged;
    unit->skill = tmpl->skill;

    return unit;
}

// --- 核心战斗循环 ---

BattleResult BattleEngine::simulate(const std::vector<std::shared_ptr<MonsterInstance>>& playerUnits, 
                                    const std::vector<std::shared_ptr<MonsterInstance>>& enemyUnits) {
    BattleResult result;
    std::vector<std::unique_ptr<BattleUnit>> units;

    // 载入双方单位
    for (const auto& pu : playerUnits) if (auto u = createBattleUnit(pu, 0)) units.push_back(std::move(u));
    for (const auto& eu : enemyUnits) if (auto u = createBattleUnit(eu, 1)) units.push_back(std::move(u));

    const int MAX_TICKS = 150;
    int currentTick = 0;

    while (currentTick < MAX_TICKS) {
        int playerAliveCount = 0;
        int enemyAliveCount = 0;
        
        // 统计存活人数
        for (const auto& u : units) {
            if (u->alive) {
                if (u->team == 0) playerAliveCount++;
                else enemyAliveCount++;
            }
        }

        // 胜负已分，提前结束推演
        if (playerAliveCount == 0 || enemyAliveCount == 0) break;

        Tick tickData;
        tickData.frame = currentTick;

        // 遍历每个单位执行动作
        for (auto& unit : units) {
            if (!unit->alive) continue;

            // 处理控制状态
            if (unit->stunTimer > 0) {
                unit->stunTimer--;
                continue;
            }

            unit->attackCooldown = std::max(0, unit->attackCooldown - 1);
            if (unit->slowTimer > 0) unit->slowTimer--;

            // 找最近敌人
            BattleUnit* closestEnemy = findClosestEnemy(unit.get(), units);
            if (!closestEnemy) continue;

            float dist = distance(unit.get(), closestEnemy);
            bool isSlowed = unit->slowTimer > 0;

            if (dist <= unit->range + 0.5f) {
                // 在攻击范围内
                if (unit->attackCooldown <= 0) {
                    if (unit->mana >= unit->maxMana) {
                        // 满蓝，释放技能 (内部生成 Actions 并结算伤害)
                        castSkill(unit.get(), units, tickData.actions);
                        unit->mana = 0;
                        unit->attackCooldown = static_cast<int>(std::ceil(1.0f / unit->atkSpeed)) + 2; // 施法后摇
                    } else {
                        // 普通攻击
                        int dmg = calcDamage(unit->atk, DamageType::Physical, closestEnemy->armor, closestEnemy->magicResist);
                        applyDamage(closestEnemy, dmg);
                        
                        // 攻击回蓝
                        unit->mana = std::min(unit->maxMana, unit->mana + 10);
                        closestEnemy->mana = std::min(closestEnemy->maxMana, closestEnemy->mana + 8);
                        
                        unit->attackCooldown = static_cast<int>(std::ceil(1.0f / unit->atkSpeed));

                        // 记录动作
                        BattleAction attackAction;
                        attackAction.type = ActionType::Attack;
                        attackAction.uid = unit->uid;
                        attackAction.targetUid = closestEnemy->uid;
                        attackAction.damage = dmg;
                        attackAction.damageType = DamageType::Physical;
                        if (unit->isRanged) {
                            attackAction.from = Position{unit->row, unit->col};
                            attackAction.to = Position{closestEnemy->row, closestEnemy->col};
                        }
                        tickData.actions.push_back(attackAction);
                    }
                }
            } else {
                // 不在范围内，需要移动
                float speed = isSlowed ? 0.5f : 1.0f;
                auto newPos = moveToward(unit.get(), closestEnemy, units, speed);
                if (newPos) {
                    BattleAction moveAction;
                    moveAction.type = ActionType::Move;
                    moveAction.uid = unit->uid;
                    moveAction.from = Position{unit->row, unit->col};
                    moveAction.to = newPos.value();
                    tickData.actions.push_back(moveAction);

                    unit->row = newPos->row;
                    unit->col = newPos->col;
                }
            }
        }

        // 记录本帧所有单位的终态
        for (const auto& u : units) {
            if (u->alive) {
                tickData.units.push_back({u->uid, u->team, u->row, u->col, u->hp, u->maxHp, 
                                          u->mana, u->maxMana, u->shield, u->alive, u->stunTimer});
            }
        }

        result.ticks.push_back(tickData);
        currentTick++;
    }

    // 结算结果
    result.playerSurvived = 0;
    result.enemySurvived = 0;
    for (const auto& u : units) {
        if (u->alive) {
            if (u->team == 0) result.playerSurvived++;
            else result.enemySurvived++;
        }
    }

    if (result.playerSurvived > 0 && result.enemySurvived == 0) result.winner = "player";
    else if (result.enemySurvived > 0 && result.playerSurvived == 0) result.winner = "enemy";
    else result.winner = "draw";

    return result;
}

// --- 移动与寻敌 ---

BattleUnit* BattleEngine::findClosestEnemy(BattleUnit* unit, const std::vector<std::unique_ptr<BattleUnit>>& allUnits) {
    BattleUnit* closest = nullptr;
    float minDist = 9999.0f;

    for (const auto& other : allUnits) {
        if (other->alive && other->team != unit->team) {
            float dist = distance(unit, other.get());
            if (dist < minDist) {
                minDist = dist;
                closest = other.get();
            }
        }
    }
    return closest;
}

std::optional<Position> BattleEngine::moveToward(BattleUnit* unit, BattleUnit* target, const std::vector<std::unique_ptr<BattleUnit>>& allUnits, float speed) {
    int dr = target->row - unit->row;
    int dc = target->col - unit->col;
    float dist = distance(unit, target);
    
    if (dist == 0.0f) return std::nullopt;

    // 四舍五入计算移动步长
    int moveDr = static_cast<int>(std::round((dr / dist) * speed));
    int moveDc = static_cast<int>(std::round((dc / dist) * speed));

    int newRow = std::clamp(unit->row + moveDr, 0, boardRows - 1);
    int newCol = std::clamp(unit->col + moveDc, 0, boardCols - 1);

    // 检查目标位置是否已被占领
    auto isOccupied = [&](int r, int c) {
        return std::any_of(allUnits.begin(), allUnits.end(), [&](const auto& u) {
            return u->alive && u->uid != unit->uid && u->row == r && u->col == c;
        });
    };

    if (isOccupied(newRow, newCol)) {
        // 尝试仅沿单轴移动 (简单避障)
        int altRow = std::clamp(unit->row + moveDr, 0, boardRows - 1);
        int altCol = std::clamp(unit->col + moveDc, 0, boardCols - 1);
        
        if (!isOccupied(altRow, unit->col) && moveDr != 0) {
            newRow = altRow;
            newCol = unit->col;
        } else if (!isOccupied(unit->row, altCol) && moveDc != 0) {
            newRow = unit->row;
            newCol = altCol;
        } else {
            return std::nullopt; // 卡住了
        }
    }

    if (newRow == unit->row && newCol == unit->col) return std::nullopt;
    return Position{newRow, newCol};
}

// 备注：castSkill 方法实现略去，它将根据 SkillData 里面的 TargetType (AoE, 单体等) 
// 遍历 allUnits 进行距离判断并结算伤害，同理生成对应的 ActionType::SkillDamage 压入 actionsOut 即可。