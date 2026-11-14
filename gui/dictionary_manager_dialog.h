#pragma once

// 词典管理对话框入口（LAY-004 自 gui/main.cpp 拆出）：装载列表、添加
// 文件/目录、启停、优先级排序、分组标签、失败隔离的重试/遗忘。MainWindow
// 定义在 main.cpp 匿名命名空间不反向暴露，刷新经回调回传。

#include <functional>

class QWidget;

// onTagChanged：分组标签变更后立即刷新主窗分组下拉；
// onFinished：对话框关闭后整体刷新主窗。
void runDictionaryManagerDialog(QWidget* parent,
                                const std::function<void()>& onTagChanged,
                                const std::function<void()>& onFinished);
