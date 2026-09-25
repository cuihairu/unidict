#pragma once
// 纯逻辑部分已下沉 core/std/pcm_util_std.h（M3b 起发音适配器也要消费
// WAV 解析，适配器不许反向依赖 gui/）。此文件保留为转发头，gui 内既有
// include "pcm_util.h" 不动。
#include "std/pcm_util_std.h"
