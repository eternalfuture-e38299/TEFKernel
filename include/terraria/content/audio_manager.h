/*******************************************************************************
 * File: audio_manager
 * Project: tefkernel
 * Created: 2026/10/29
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 *
 * MIT License
 *
 * Copyright (c) 2026 eternalfuture-e38299
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/

#ifndef TEFKERNEL_AUDIO_MANAGER_H
#define TEFKERNEL_AUDIO_MANAGER_H

#include "../../tef_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 一个可播放的音源（解码后的 PCM 数据及其元信息）。
 *
 * @details
 * 该结构体由**调用方拥有并负责生命周期**。音频管理器只保存指向它的指针
 * （register/play 时），不会拷贝其内容，也不会释放它。
 *
 * 一旦某个 voice 正在播放它（包括暂停中），就不得释放该结构体或其
 * #pcm_data 指向的内存，否则音频回调线程会读到悬垂指针。
 *
 * 所有字段应在调用 #terraria_audio_manager_register /
 * #terraria_audio_manager_play 之前设置完毕，登记后**不要修改**
 * （尤其是 `pcm_data` / `pcm_frames` / `format` / `channels` / `sample_rate`，
 * 回调线程会直接读取这些字段）。
 */
typedef struct terraria_audio_t {
    /**
     * @brief 调试/归属标识，可为 `NULL`。
     * @note 不参与播放逻辑，仅用于日志与排查。
     */
    const char *source_name;

    /**
     * @brief 音源采样率，单位 Hz（例如 44100、48000）。
     * @warning 当前实现按"设备采样率 == 音源采样率"播放，未做重采样。
     *          若与设备采样率不一致，播放速度/音高会失真。必须 > 0。
     */
    int sample_rate;

    /**
     * @brief 声道数，1（单声道）或 2（立体声）。
     * @note 回调会对输出声道做映射：输出声道数大于音源声道数时，
     *       多出的输出声道复制最后一个源声道（即单声道会复制到左右声道）。
     *       必须 > 0。
     */
    int channels;

    /**
     * @brief 采样格式，取值对应 miniaudio 的 `ma_format` 枚举：
     *        - 1 = `ma_format_u8`  （无符号 8 位）
     *        - 2 = `ma_format_s16` （有符号 16 位）
     *        - 3 = `ma_format_s24` （有符号 24 位，紧凑 3 字节）
     *        - 4 = `ma_format_s32` （有符号 32 位）
     *        - 5 = `ma_format_f32` （32 位浮点，推荐）
     * @note 内部会归一化到 [-1.0, 1.0] 的 float 再混音。非法值会被 register 拒绝。
     */
    int format;

    /**
     * @brief 指向 PCM 数据的指针，布局为**交错(interleaved)**：
     *        `frame0[ch0], frame0[ch1], frame1[ch0], ...`。
     * @note 内存由调用方分配/释放；管理器只读不写。
     *       不得为 `NULL`（register 会拒绝）。
     */
    const void *pcm_data;

    /**
     * @brief PCM 总帧数（一帧 = 所有声道各一个采样）。
     * @note 换算：`pcm_frames = 总字节数 / (channels * bytes_per_sample)`。
     *       必须 > 0（register 会拒绝）。
     */
    unsigned long long pcm_frames;

    /**
     * @brief 是否循环播放。
     * @note `true` 时播放到结尾自动回到第 0 帧继续；`false` 时自然播完，
     *       对应 voice 转入 `VOICE_STOPPING` 并在下一回调被回收。
     */
    bool looping;

    /**
     * @brief 该音源的默认音量，范围 `0.0 ~ 1.0`。
     * @note 仅在调用 #terraria_audio_manager_play 时 `volume` 传 `-1`（或 <0）
     *       才会使用它。若登记时该值 <= 0，register 会把它纠正为 1.0。
     */
    float volume;
} terraria_audio_t;

/**
 * @brief 登记一个音源，使其可被 #terraria_audio_manager_play 使用。
 *
 * @param audio 音源指针；必须非空且字段合法：
 *              `pcm_data != NULL`、`pcm_frames > 0`、`sample_rate > 0`、
 *              `channels > 0`、`format` 为 1..5 之一。
 *              若 `volume <= 0` 会被自动设为 1.0。
 *
 * @return 成功（首次登记，或已登记过——幂等）返回 `true`；
 *         参数非法或内部初始化失败返回 `false`。
 *
 * @note
 *   - 同一 `audio` 重复登记是幂等的，不会产生重复条目。
 *   - 登记仅保存指针，不拷贝 PCM。音源内存的所有权仍在调用方。
 *   - 线程安全。
 *
 * @see terraria_audio_manager_unregister
 */
DEFINE_FUNCTION(bool, terraria_audio_manager_register, terraria_audio_t *audio)

/**
 * @brief 注销音源，并请求停止其所有 voice。
 *
 * @param audio 之前登记过的音源指针。
 *
 * @return 若该音源确实在登记表中（已移除）返回 `true`；未找到或参数非法返回 `false`。
 *
 * @warning
 *   本函数内部调用 #terraria_audio_manager_stop_all（仅设置停止标志），
 *   **不会等待音频回调线程真正回收 voice**。由于回调线程直接读取
 *   `audio->pcm_data`，若调用方在 unregister 返回后立即释放音源内存，
 *   可能与仍在进行的那一次回调产生**数据竞争 / 悬垂指针**。
 *   安全做法：unregister 后轮询相关 voice 的
 *   #terraria_audio_manager_is_playing 直到为 false，或延迟一段时间再释放
 *   音源内存；也可在释放前调用 #terraria_audio_manager_shutdown。
 *
 * @note 线程安全。
 */
DEFINE_FUNCTION(bool, terraria_audio_manager_unregister, terraria_audio_t *audio)

/**
 * @brief 开始一次播放，返回该播放实例的 voice_id。
 *
 * @param audio  要播放的音源。建议先 register；未 register 时若参数合法
 *               也会被隐式使用（实现上 play 不强制要求已登记）。
 * @param volume 本次播放的音量，范围 `0.0 ~ 1.0`。
 *               传负数（约定 `-1`）表示使用 `audio->volume`。
 * @param pitch  播放速率/音高，`1.0` = 原速。
 *               传 `<= 0` 会被纠正为 `1.0`。
 *               @warning 当前实现对变速步长做整数截断，非 1.0 的 pitch
 *               可能导致实际速度与预期不符（详见实现说明）。
 *
 * @return 成功返回 `>= 0` 的 voice_id；失败（参数空、内部初始化失败、
 *         设备无法打开、槽位分配失败）返回 `-1`。
 *
 * @note
 *   - 首次调用会惰性初始化音频设备（可能耗时数十毫秒）。
 *   - voice_id 可能被后续播放复用，见文件头"voice_id 语义"。
 *   - 线程安全。
 *
 * @see terraria_audio_manager_stop
 */
DEFINE_FUNCTION(int, terraria_audio_manager_play, terraria_audio_t *audio,
                float volume, float pitch)

/**
 * @brief 暂停指定 voice 的播放（保留播放位置）。
 *
 * @param voice_id 由 #terraria_audio_manager_play 返回的 id。
 *
 * @note
 *   - 仅当 voice 处于 `VOICE_ACTIVE` 时有效；否则为空操作。
 *   - 暂停期间回调跳过该 voice，不推进播放位置，也不消耗 CPU 混音。
 *   - 调用后到真正静音之间存在至多一个音频周期的延迟。
 *   - 线程安全。
 *
 * @see terraria_audio_manager_resume
 */
DEFINE_FUNCTION(void, terraria_audio_manager_pause, int voice_id)

/**
 * @brief 恢复被 #terraria_audio_manager_pause 暂停的 voice。
 *
 * @param voice_id 播放实例 id。
 *
 * @note 仅当 voice 仍为 `VOICE_ACTIVE` 时有效；否则为空操作。线程安全。
 */
DEFINE_FUNCTION(void, terraria_audio_manager_resume, int voice_id)

/**
 * @brief 请求停止指定 voice（异步，不立即释放槽位）。
 *
 * @param voice_id 播放实例 id。
 *
 * @note
 *   - 停止是**异步**的：本函数只设置停止标志，voice 在下一次音频回调中
 *     被静音，并在随后的回调中转回 `VOICE_FREE`。
 *   - 返回后立即调用 #terraria_audio_manager_is_playing 可能仍短暂返回
 *     true（取决于是否已跨过一个音频周期）。
 *   - 线程安全。
 */
DEFINE_FUNCTION(void, terraria_audio_manager_stop, int voice_id)

/**
 * @brief 设置单个 voice 的播放音量（实时生效）。
 *
 * @param voice_id 播放实例 id。
 * @param volume   目标音量，会被钳制到 `0.0 ~ 1.0`。
 *
 * @note 与 #terraria_audio_manager_set_master_volume 相乘得到最终增益。
 *       voice 非活跃时为空操作。线程安全。
 */
DEFINE_FUNCTION(void, terraria_audio_manager_set_volume, int voice_id, float volume)

/**
 * @brief 查询指定 voice 当前是否"正在发声"。
 *
 * @param voice_id 播放实例 id。
 *
 * @return 当且仅当 voice 处于 `VOICE_ACTIVE` 且**未暂停**且**未请求停止**
 *         时返回 `true`；否则返回 `false`（包括 id 非法、已回收、暂停中、
 *         正在停止）。
 *
 * @note 这是判断"一次播放是否还活着"的推荐方式。
 *       注意：返回 false 不代表 id 立即失效——槽位可能还没被新播放复用，
 *       但语义上应视为该次播放已结束。线程安全。
 */
DEFINE_FUNCTION(bool, terraria_audio_manager_is_playing, int voice_id)

/**
 * @brief 请求停止某一音源的所有 voice。
 *
 * @param audio 目标音源指针。
 *
 * @note 与 #terraria_audio_manager_stop 一样是异步停止；不影响其它音源的 voice。
 *       线程安全。
 */
DEFINE_FUNCTION(void, terraria_audio_manager_stop_all, terraria_audio_t *audio)

/**
 * @brief 设置全局主音量，作用于所有 voice 的最终输出。
 *
 * @param volume 目标主音量，会被钳制到 `0.0 ~ 1.0`。
 *
 * @note 最终增益 = `voice_volume * master_volume`。
 *       可在任意时刻调用，无锁、实时生效（每音频周期读取一次）。
 *       线程安全。
 */
DEFINE_FUNCTION(void, terraria_audio_manager_set_master_volume, float volume)

#if IS_TEFKERNEL_BUILD
/**
 * @brief 关闭音频子系统（仅内核构建可用，通常在进程退出时调用）。
 *
 * @details
 * 依次：停止并反初始化音频设备 → 释放所有 voice 槽位 → 释放音源登记表
 * → 销毁互斥量。调用后除再次初始化（如再次 play）外，本模块 API 均变为
 * 安全空操作。
 *
 * @warning 必须在所有使用本模块的线程都已停止、或至少不再调用本模块 API
 *          之后调用；本函数不负责同步外部线程。
 */
void terraria_audio_manager_shutdown();
#endif

#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_AUDIO_MANAGER_H