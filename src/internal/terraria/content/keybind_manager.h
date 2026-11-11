/*******************************************************************************
 * tefkernel - keybind_manager (internal)
 *******************************************************************************/
#ifndef TEFKERNEL_INTERNAL_KEYBIND_MANAGER_H
#define TEFKERNEL_INTERNAL_KEYBIND_MANAGER_H

#include "terraria/content/keybind_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 安装 PC 键位注入所需的类型解析与钩子（幂等）。
 * 在桌面运行时初始化阶段调用；类型尚未就绪时会在首次可用调用时重试。 */
void terraria_keybind_manager_init(void);

/* 卸载键位管理器：移除钩子并释放内部句柄。
 * 已注入到游戏 KeyStatus 的条目会保留到本次游戏进程结束（避免正在读取菜单时删除）。 */
void terraria_keybind_manager_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* TEFKERNEL_INTERNAL_KEYBIND_MANAGER_H */
