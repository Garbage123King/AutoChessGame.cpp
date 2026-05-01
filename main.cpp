#include <iostream>
#include <string>
#include "GameManager.h"
#include "GameConfig.h"
// --- 新增：专门为了解决 Windows 控制台中文乱码 ---
#ifdef _WIN32
#include <windows.h>
#endif
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

    // 3. 部署到棋盘 (玩家阵地在 4-7 行)
    std::cout << "\n--- 部署阶段 ---" << std::endl;
    // 将备战区第 0 个怪放到 (4, 4)
    if (game.benchToBoard(0, 4, 4)) {
        std::cout << "成功将棋子部署到 (4, 4)" << std::endl;
    }
    // 将现在的备战区第 0 个怪（原第 1 个）放到 (4, 5)
    if (!player->bench.empty()) {
        if (game.benchToBoard(0, 4, 5)) {
            std::cout << "成功将棋子部署到 (4, 5)" << std::endl;
        }
    }

    std::cout << "部署后棋盘数量: " << player->board.size() << std::endl;

    // 4. 开始战斗推演
    std::cout << "\n--- 战斗推演开始 ---" << std::endl;
    BattleResult result = game.startBattle(true); // true 表示需要详细的 Ticks 录像

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