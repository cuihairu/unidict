> 语言: **简体中文** | [English](../../design/README.md)

# 设计草稿（非正式既定方向）

本目录下的所有材料均为**设计阶段产物**。其中没有任何内容已获实施决议；
仓库中也没有任何代码实现这些文档。

- `sync-engine.md` — 云级同步引擎设计（账号、增量同步、端到端加密）。
  状态：仅设计定稿。当前已发布的同步是 `adapters/qt/sync_service_qt`
  中的本地文件级合并（无云、无账号），保留为无账号回退路径。
- `text-normalization.md` — 文本归一化设计笔记。

行动准则（与 `docs/server_plan.md` 相同）：**未经明确的产品决策，不得从
这些草稿开始实施。** 多设备云同步与词典服务器是 roadmap 上暂缓的项目
（"deferred — user-driven only"）；产品基线保持本地优先。定位说明见
`docs/product-principles.md`，当前实际交付的功能面见
`docs/CURRENT_FEATURE_MATRIX.md`。
