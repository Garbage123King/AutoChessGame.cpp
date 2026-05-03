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
    //unit->row = instance->row;
    //unit->col = instance->col;
    unit->row = static_cast<float>(instance->row);
    unit->col = static_cast<float>(instance->col);

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
    std::vector<Projectile> activeProjectiles;

    // 载入双方单位
    for (const auto& pu : playerUnits) if (auto u = createBattleUnit(pu, 0)) units.push_back(std::move(u));
    for (const auto& eu : enemyUnits) if (auto u = createBattleUnit(eu, 1)) units.push_back(std::move(u));

    // 最大战斗时间 30 秒，乘以 30 TPS = 900 帧
    const int MAX_TICKS = 30 * TPS;
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

        // 1. 处理所有天上的弹道
        for (auto& proj : activeProjectiles) {
            if (proj.hit) continue;

            // 实时追踪目标
            BattleUnit* target = findUnitByUid(proj.targetUid, units);
            if (target && target->alive) {
                proj.targetPos = { target->row, target->col };
            }

            // 计算飞向目标的向量
            float dr = proj.targetPos.row - proj.currentPos.row;
            float dc = proj.targetPos.col - proj.currentPos.col;
            float dist = std::sqrt(dr * dr + dc * dc);

            if (dist <= proj.speed) {
                // 命中目标！
                proj.hit = true;
                if (target && target->alive) {
                    applyDamage(target, proj.damage); // 扣血
                    target->mana = std::min(target->maxMana, target->mana + 8); // 受击回蓝

                    // 记录命中动作给 Godot
                    BattleAction hitAction;
                    hitAction.type = ActionType::ProjectileHit;
                    hitAction.uid = proj.uid;
                    hitAction.targetUid = proj.targetUid;
                    hitAction.damage = proj.damage;
                    tickData.actions.push_back(hitAction);
                }
            }
            else {
                // 飞行中
                proj.currentPos.row += (dr / dist) * proj.speed;
                proj.currentPos.col += (dc / dist) * proj.speed;
            }
        }

        // 清理已命中的弹道
        activeProjectiles.erase(std::remove_if(activeProjectiles.begin(), activeProjectiles.end(),
            [](const Projectile& p) { return p.hit; }), activeProjectiles.end());

        // 2. 更新所有单位的状态机
        for (auto& unit : units) {
            if (!unit->alive) continue;

            // 眩晕状态优先
            if (unit->state == UnitState::Stunned) {
                unit->stateTicks--;
                if (unit->stateTicks <= 0) unit->state = UnitState::Idle;
                continue;
            }

            switch (unit->state) {
                case UnitState::Idle: {
                    BattleUnit* target = findClosestEnemy(unit.get(), units);
                    if (!target) break;

                    float dist = distance(unit.get(), target);
                    if (dist <= unit->range + 0.5f) {
                        // 在攻击范围内，开始前摇！
                        unit->currentTargetUid = target->uid;
                        unit->state = UnitState::Windup;
                        
                        // 攻速换算公式：总 Tick = (1.0 / 攻速) * 30
                        int totalAttackTicks = std::round((1.0f / unit->atkSpeed) * TPS);
                        // 假设前摇占 30%
                        unit->stateTicks = std::round(totalAttackTicks * 0.3f); 
                        
                        // 记录动作给 Godot 播放“抬手动画”
                        BattleAction windupAct;
                        windupAct.type = ActionType::Windup;
                        windupAct.uid = unit->uid;
                        windupAct.targetUid = target->uid;
                        tickData.actions.push_back(windupAct);
                        
                    } else {
                        // 不在范围内，进入移动状态
                        auto nextPosOpt = calculateNextPath(unit.get(), target, units); // 寻路算法返回下一个格子
                        if (nextPosOpt) {
                            unit->state = UnitState::Moving;
                            unit->nextMovePos = nextPosOpt.value();
                            unit->stateTicks = 15; // 假设移动一格固定需要 15 Ticks (0.5秒)
                        }
                    }
                    break;
                }

                case UnitState::Windup: {
                    unit->stateTicks--;
                    if (unit->stateTicks <= 0) {
                        // 前摇结束！打出伤害或射出弹道
                        BattleUnit* target = findUnitByUid(unit->currentTargetUid, units);
                        if (target && target->alive) {
                            int dmg = calcDamage(unit->atk, DamageType::Physical, target->armor, target->magicResist);
                            
                            if (unit->isRanged) {
                                // 远程：生成弹道
                                Projectile proj;
                                proj.uid = "proj_" + std::to_string(currentTick) + "_" + unit->uid;
                                proj.sourceUid = unit->uid;
                                proj.targetUid = target->uid;
                                proj.currentPos = {unit->row, unit->col};
                                proj.targetPos = {target->row, target->col};
                                proj.speed = 15.0f / TPS; // 假设每秒飞 15 格
                                proj.damage = dmg;
                                activeProjectiles.push_back(proj);
                            } else {
                                // 近战：瞬间扣血
                                applyDamage(target, dmg);
                                // 近战打击特效动作...
                            }
                            unit->mana = std::min(unit->maxMana, unit->mana + 10); // 攻击回蓝
                        }

                        // 进入后摇冷却
                        unit->state = UnitState::Backswing;
                        int totalAttackTicks = std::round((1.0f / unit->atkSpeed) * TPS);
                        int windupTicks = std::round(totalAttackTicks * 0.3f);
                        unit->stateTicks = totalAttackTicks - windupTicks; // 剩下的 70% 是后摇
                    }
                    break;
                }

                case UnitState::Backswing: {
                    unit->stateTicks--;
                    if (unit->stateTicks <= 0) {
                        unit->state = UnitState::Idle; // 冷却结束，可以进行下一次行动
                    }
                    break;
                }

                case UnitState::Moving: {
                    // 平滑移动到目标格子
                    unit->stateTicks--;
                    float progress = 1.0f - (float)unit->stateTicks / 15.0f; // 15是设定的移动总Tick
                    // 插值计算当前平滑坐标...
                    
                    if (unit->stateTicks <= 0) {
                        unit->row = unit->nextMovePos.row;
                        unit->col = unit->nextMovePos.col;
                        unit->state = UnitState::Idle; // 走完一格，重新寻敌
                    }
                    break;
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
    float dr = target->row - unit->row;
    float dc = target->col - unit->col;
    float dist = distance(unit, target);

    if (dist == 0.0f) return std::nullopt;

    // 根据速度计算浮点移动步长
    float moveDr = (dr / dist) * speed;
    float moveDc = (dc / dist) * speed;

    float maxRow = static_cast<float>(boardRows - 1);
    float maxCol = static_cast<float>(boardCols - 1);

    // 强制声明使用 <float> 进行 clamp
    float newRow = std::clamp<float>(unit->row + moveDr, 0.0f, maxRow);
    float newCol = std::clamp<float>(unit->col + moveDc, 0.0f, maxCol);

    // 检查目标位置是否已被占领 (浮点数近似判断，小于 0.1 视作重合)
    auto isOccupied = [&](float r, float c) {
        return std::any_of(allUnits.begin(), allUnits.end(), [&](const auto& u) {
            return u->alive && u->uid != unit->uid && std::abs(u->row - r) < 0.1f && std::abs(u->col - c) < 0.1f;
            });
        };

    if (isOccupied(newRow, newCol)) {
        // 尝试仅沿单轴移动 (简单避障)
        float altRow = std::clamp<float>(unit->row + moveDr, 0.0f, maxRow);
        float altCol = std::clamp<float>(unit->col + moveDc, 0.0f, maxCol);

        if (!isOccupied(altRow, unit->col) && std::abs(moveDr) > 0.01f) {
            newRow = altRow;
            newCol = unit->col;
        }
        else if (!isOccupied(unit->row, altCol) && std::abs(moveDc) > 0.01f) {
            newRow = unit->row;
            newCol = altCol;
        }
        else {
            return std::nullopt; // 卡住了
        }
    }

    if (std::abs(newRow - unit->row) < 0.01f && std::abs(newCol - unit->col) < 0.01f) {
        return std::nullopt;
    }

    return Position{ newRow, newCol };
}

// 备注：castSkill 方法实现略去，它将根据 SkillData 里面的 TargetType (AoE, 单体等) 
// 遍历 allUnits 进行距离判断并结算伤害，同理生成对应的 ActionType::SkillDamage 压入 actionsOut 即可。
// ==================== BattleEngine.cpp 底部补充 ====================

// 快速模拟：直接调用正常模拟，但丢弃录像帧数据，节省内存
BattleResult BattleEngine::quickSimulate(const std::vector<std::shared_ptr<MonsterInstance>>& playerUnits, 
                                         const std::vector<std::shared_ptr<MonsterInstance>>& enemyUnits) {
    BattleResult result = simulate(playerUnits, enemyUnits);
    result.ticks.clear(); // 抛弃推演录像
    return result;
}

// 释放技能逻辑
void BattleEngine::castSkill(BattleUnit* caster, const std::vector<std::unique_ptr<BattleUnit>>& allUnits, std::vector<BattleAction>& actionsOut) {
    BattleAction action;
    action.type = ActionType::Cast;
    action.uid = caster->uid;
    action.skillName = caster->skill.name;
    action.damageType = caster->skill.damageType;
    action.from = Position{caster->row, caster->col};

    // 简单实现：找到最近的敌人打出技能伤害
    BattleUnit* target = findClosestEnemy(caster, allUnits);
    if (target) {
        action.targetUid = target->uid;
        
        // 伤害计算 (技能基础伤害)
        int actualDmg = calcDamage(caster->skill.damage, caster->skill.damageType, target->armor, target->magicResist);
        
        applyDamage(target, actualDmg);
        action.damage = actualDmg;
        
        // 记录技能附带的控制效果 (比如眩晕)
        if (caster->skill.stunDuration > 0 && target->alive) {
            target->stunTimer = caster->skill.stunDuration;
            
            BattleAction stunAction;
            stunAction.type = ActionType::Stun;
            stunAction.uid = caster->uid;
            stunAction.targetUid = target->uid;
            stunAction.amount = caster->skill.stunDuration;
            actionsOut.push_back(stunAction);
        }
    }

    actionsOut.push_back(action);
}

BattleUnit* BattleEngine::findUnitByUid(const std::string& uid, const std::vector<std::unique_ptr<BattleUnit>>& allUnits) {
    for (const auto& u : allUnits) {
        if (u->uid == uid) return u.get();
    }
    return nullptr;
}

std::optional<Position> BattleEngine::calculateNextPath(BattleUnit* unit, BattleUnit* target, const std::vector<std::unique_ptr<BattleUnit>>& allUnits) {
    float dr = target->row - unit->row;
    float dc = target->col - unit->col;
    float dist = std::sqrt(dr * dr + dc * dc);

    if (dist == 0.0f) return std::nullopt;

    // 每次移动 1 格
    float moveDr = std::round(dr / dist);
    float moveDc = std::round(dc / dist);

    // 修复 std::clamp 报错：确保三个参数全都是 float
    float maxRow = static_cast<float>(boardRows - 1);
    float maxCol = static_cast<float>(boardCols - 1);

    float newRow = std::clamp(unit->row + moveDr, 0.0f, maxRow);
    float newCol = std::clamp(unit->col + moveDc, 0.0f, maxCol);

    // 碰撞检测：检查新位置是否有其他活着的单位
    bool isOccupied = std::any_of(allUnits.begin(), allUnits.end(), [&](const auto& u) {
        if (!u->alive || u->uid == unit->uid) return false;
        // 浮点数坐标近似判断（距离小于0.1认为重合）
        float dRow = u->row - newRow;
        float dCol = u->col - newCol;
        return (dRow * dRow + dCol * dCol) < 0.1f;
        });

    if (isOccupied) {
        // 简单避障：尝试只走横向或纵向
        float altRow = std::clamp(unit->row + moveDr, 0.0f, maxRow);
        float altCol = std::clamp(unit->col + moveDc, 0.0f, maxCol);

        bool rowOccupied = std::any_of(allUnits.begin(), allUnits.end(), [&](const auto& u) {
            return u->alive && u->uid != unit->uid && std::abs(u->row - altRow) < 0.1f && std::abs(u->col - unit->col) < 0.1f;
            });

        bool colOccupied = std::any_of(allUnits.begin(), allUnits.end(), [&](const auto& u) {
            return u->alive && u->uid != unit->uid && std::abs(u->row - unit->row) < 0.1f && std::abs(u->col - altCol) < 0.1f;
            });

        if (!rowOccupied && moveDr != 0.0f) {
            newRow = altRow;
            newCol = unit->col;
        }
        else if (!colOccupied && moveDc != 0.0f) {
            newRow = unit->row;
            newCol = altCol;
        }
        else {
            return std::nullopt; // 被彻底卡住
        }
    }

    if (std::abs(newRow - unit->row) < 0.01f && std::abs(newCol - unit->col) < 0.01f) {
        return std::nullopt;
    }

    return Position{ newRow, newCol };
}