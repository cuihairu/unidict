> 语言: **简体中文** | [English](../../design/README.md)

# 设计草稿（非正式既定方向）

本目录下的所有材料均为**设计阶段产物**，且两份设计都未以完整形态获实施
决议。但两者均已有缩略形态落地：B 系列（binlog 中转、动态密码配对、端到
端密文、默认关闭）是 `sync-engine.md` 的已发布实现，三档 profile 文本归
一化引擎（2026-10-16 交付）是 `text-normalization.md` 的已发布实现。

- `sync-engine.md` — 云级同步引擎设计（账号、增量同步、端到端加密）。
  状态：仅设计定稿；以缩略形态落地为 B 系列——`sync_manager_qt` +
  `core/std/sync_engine_std`（binlog 指令流、默认关闭），
  `adapters/qt/sync_service_qt` 中的本地文件级合并（无云、无账号）
  保留为无账号回退路径。
- `text-normalization.md` — 文本归一化设计笔记；已实现为三档 profile
  文本归一化引擎（`core/std/text_norm_std.*`）。

行动准则（与 `docs/server_plan.md` 相同）：**未经明确的产品决策，不得从
这些草稿开始实施。** 多设备云同步与词典服务器是 roadmap 上暂缓的项目
（"deferred — user-driven only"）；产品基线保持本地优先。定位说明见
`docs/product-principles.md`，当前实际交付的功能面见
`docs/CURRENT_FEATURE_MATRIX.md`。
