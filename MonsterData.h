#pragma once
#include <string>
#include "Enums.h"

// 技能数据模板
struct SkillData {
    std::string name;
    std::string description;
    DamageType damageType;
    int damage;
    TargetType targetType;
    int manaCost;

    // 特殊效果（默认为 0，表示没有该效果）
    int heal = 0;
    int aoeRadius = 0;
    int shield = 0;
    int stunDuration = 0;
    int slowDuration = 0;
    bool isAssassin = false; 
};

// 怪物静态数据模板 (对应图鉴中的基础数据)
struct MonsterData {
    std::string id;
    std::string name;
    std::string title;
    int cost;
    
    int baseHp;
    int baseAtk;
    float atkSpeed;
    int range;
    
    int armor;
    int magicResist;
    int maxMana;
    int startMana;
    
    bool isRanged;
    std::string color;
    
    SkillData skill;
};

// 战斗中的怪物实例 (区分于静态数据，加入等级、当前状态等)
struct MonsterInstance {
    std::string uid;     // 唯一标识符
    std::string id;      // 指向基础数据 MonsterData
    int star = 1;        // 星级 (1, 2, 3)
    
    // 战斗状态（仅在战斗引擎中有效）
    int row = -1;
    int col = -1;
};