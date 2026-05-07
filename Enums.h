#pragma once

enum class DamageType {
    Physical,
    Magic,
    TrueDamage // <-- 真实伤害
};

enum class TargetType {
    Single,
    Line,
    Aoe
};