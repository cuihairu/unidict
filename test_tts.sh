#!/bin/bash

# 设置词典路径
export UNIDICT_DICTS="/Users/cui/Workspaces/unidict/examples/test_dict.json"

# 启动应用
echo "🚀 启动 UniDict QML 应用 (带 TTS 语音播放功能)"
echo "📖 加载词典: $UNIDICT_DICTS"
echo ""
echo "🎯 测试步骤:"
echo "1. 切换到'🔊语音'标签页查看语音设置"
echo "2. 在'Search'标签页搜索单词如'hello'"
echo "3. 点击'🔊播放'按钮播放单词发音"
echo "4. 点击'📖定义'按钮播放定义内容"
echo "5. 在语音设置中调整音量、语速、音调"
echo ""
echo "按 Ctrl+C 停止应用"
echo ""

cd /Users/cui/Workspaces/unidict
./build/qmlui/unidict_qml