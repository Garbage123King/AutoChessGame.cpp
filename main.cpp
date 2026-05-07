#include <iostream>
#include <string>
#include "GameManager.h"
#include "GameConfig.h"
// --- 新增：专门为了解决 Windows 控制台中文乱码 ---
#ifdef _WIN32
#include <windows.h>
#endif
#include <iomanip> // <-- 新增这个，用于 std::setprecision 格式化时间
#include <string>  // <--- 新增这一行！教 std::cout 如何打印 std::string
#include <cstdio>  // 新增：引入 printf
#include <fstream>

void exportBattleToJson(const BattleResult& result, const std::string& filename) {
    // 魔法发生的地方：一句话直接把整个 result 对象塞进 json
    nlohmann::json j = result;

    // 打开文件准备写入
    std::ofstream o(filename);

    // setw(4) 是为了输出带有 4 个空格缩进的漂亮格式，而不是挤成一团的乱码
    o << std::setw(4) << j << std::endl;

    std::cout << "\n💾 [系统提示] 战斗录像已成功保存至: " << filename << std::endl;
}

// --- 专门用于解析 30TPS 录像的战斗日志打印机 (printf 终极稳定版) ---
void printBattleLog(const BattleResult& result, int maxLines = 100) {
    printf("\n========== 📜 详细战斗日志 (Combat Log) ==========\n");
    printf("战斗总耗时: %zu 帧 (%.2f 秒)\n", result.ticks.size(), result.ticks.size() / 30.0f);
    printf("--------------------------------------------------\n");

    int linesPrinted = 0;

    // 遍历每一帧的录像
    for (const auto& tick : result.ticks) {
        if (tick.actions.empty()) continue;

        float timeSec = tick.frame / 30.0f;

        for (const auto& action : tick.actions) {
            if (linesPrinted >= maxLines) {
                printf("... (日志太长，为防止卡顿已截断，可调大 maxLines)\n");
                return;
            }

            // 格式化前缀: [00.33s | 帧  10]
            printf("[%05.2fs | 帧 %3d] ", timeSec, tick.frame);

            // 翻译动作类型 (彻底抛弃 cout，使用 %s 和 %d 占位符)
            switch (action.type) {
            case ActionType::Windup:
                printf("⚔️ %s 开始蓄力瞄准 -> %s\n", action.uid.c_str(), action.targetUid.c_str());
                break;
            case ActionType::SpawnProjectile:
                printf("🏹 %s 射出弹道 -> 飞向 %s\n", action.uid.c_str(), action.targetUid.c_str());
                break;
            case ActionType::ProjectileHit:
                printf("💥 %s 的弹道命中 %s，造成了 %d 点物理伤害！\n", action.uid.c_str(), action.targetUid.c_str(), action.damage);
                break;
            case ActionType::Cast:
                printf("✨ %s 施放了技能【%s】！\n", action.uid.c_str(), action.skillName.c_str());
                break;
            case ActionType::Stun:
                printf("💫 %s 被眩晕了！\n", action.targetUid.c_str());
                break;
            default:
                printf("❓ %s 执行了未知的动作 (代码: %d)\n", action.uid.c_str(), static_cast<int>(action.type));
                break;
            }
            linesPrinted++;
        }
    }
    printf("==================================================\n\n");
}

// 辅助函数：打印动作类型
std::string getActionName(ActionType type) {
    switch(type) {
        case ActionType::Move: return "Move";
        case ActionType::Attack: return "Attack";
        case ActionType::Cast: return "Cast";
        case ActionType::Heal: return "Heal";
        case ActionType::Shield: return "Shield";
        case ActionType::SkillDamage: return "SkillDamage";
        default: return "Unknown";
    }
}

int main() {
    // --- 新增：启动时强制控制台使用 UTF-8 ---
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "=== 自走棋 C++ 核心引擎测试启动 ===" << std::endl;

    // 1. 初始化游戏
    GameManager game("测试玩家");
    auto player = game.getPlayer();
    
    std::cout << "玩家 [" << player->name << "] 已创建 | 金币: " << player->gold << " | 血量: " << player->hp << std::endl;

    // 2. 查看初始商店并购买
    // std::cout << "\n--- 购买阶段 ---" << std::endl;
    // for (size_t i = 0; i < player->shop.size(); i++) {
    //     if (player->shop[i]) {
    //         std::cout << "商店槽位 " << i << ": " << player->shop[i]->name 
    //                   << " (" << player->shop[i]->id << ")" << std::endl;
    //     }
    // }
    // 2. 查看初始商店并购买
    std::cout << "\n--- 购买阶段 ---" << std::endl;
    for (size_t i = 0; i < player->shop.size(); i++) {
        if (player->shop[i]) {
            // 通过实例的 id 去 GameConfig 查找基础模板数据
            const auto* tmpl = GameConfig::getInstance().getMonsterTemplate(player->shop[i]->id);
            std::cout << "商店槽位 " << i << ": " << (tmpl ? tmpl->name : "未知") 
                      << " (" << player->shop[i]->id << ")" << std::endl;
        }
    }

    // 假设玩家买了第 0 个和第 1 个怪（如果有的话）
    game.buyMonster(0);
    game.buyMonster(1);
    
    std::cout << "购买后备战区数量: " << player->bench.size() << " | 剩余金币: " << player->gold << std::endl;

    game.buyXp();

    std::cout << "购买经验后玩家等级: " << player->level << " | 剩余金币: " << player->gold << std::endl;

    // 3. 部署到棋盘 (玩家阵地在 4-7 行)
    std::cout << "\n--- 部署阶段 ---" << std::endl;
    // 将备战区第 0 个怪放到 (4, 4)
    if (game.benchToBoard(0, 4, 4)) {
        std::cout << "成功将棋子部署到 (4, 4)" << std::endl;
    }
    // 将现在的备战区第 0 个怪（原第 1 个）放到 (4, 5)
    if (!player->bench.empty()) {
        if (game.benchToBoard(1, 4, 5)) {
            std::cout << "成功将棋子部署到 (4, 5)" << std::endl;
        }
    }

    std::cout << "部署后棋盘数量: " << player->board.size() << std::endl;

    // 4. 开始战斗推演
    std::cout << "\n--- 战斗推演开始 ---" << std::endl;
    BattleResult result = game.startBattle(true); // true 表示需要详细的 Ticks 录像

    // --- 新增：打印刚刚发生的战斗日志 ---
    printBattleLog(result, 200); // 最多打印 200 条动作记录

    // --- 新增：把录像导出成 JSON 文件 ---
    exportBattleToJson(result, "battle_record.json");

    std::cout << "战斗结束! 胜者: " << result.winner << std::endl;
    std::cout << "玩家存活: " << result.playerSurvived << " | 敌人存活: " << result.enemySurvived << std::endl;
    std::cout << "总计经过帧数(Ticks): " << result.ticks.size() << std::endl;

    // 5. 抽取并打印前几帧的战斗录像，验证动作
    std::cout << "\n--- 截取前 10 帧的战斗日志 ---" << std::endl;
    int printLimit = std::min(10, (int)result.ticks.size());
    for (int i = 0; i < printLimit; i++) {
        const auto& tick = result.ticks[i];
        if (!tick.actions.empty()) {
            std::cout << "[Frame " << tick.frame << "]" << std::endl;
            for (const auto& action : tick.actions) {
                std::cout << "  -> " << action.uid << " 执行了 " << getActionName(action.type);
                if (!action.targetUid.empty()) std::cout << " 目标: " << action.targetUid;
                if (action.damage.has_value()) std::cout << " 造成伤害: " << action.damage.value();
                std::cout << std::endl;
            }
        }
    }

    std::cout << "\n结算后玩家金币: " << player->gold << " | 血量: " << player->hp << " | 当前回合: " << game.getRound() << std::endl;

    return 0;
}