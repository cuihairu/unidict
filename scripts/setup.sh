#!/bin/bash
# 快速初始化脚本

set -e

echo "🚀 UniDict 项目快速初始化..."

# 创建基础目录结构
echo "📁 创建目录结构..."
./scripts/manage.sh setup

# 初始化Git仓库（如果还没有）
if [ ! -d ".git" ]; then
    echo "🔧 初始化Git仓库..."
    git init
    git add .
    git commit -m "Initial commit: UniDict monorepo structure"
fi

echo "✅ 初始化完成！"
echo ""
echo "下一步操作："
echo "1. 启动开发环境: ./scripts/manage.sh dev"
echo "2. 构建项目: ./scripts/manage.sh build"
echo "3. 运行测试: ./scripts/manage.sh test"
echo ""
echo "详细帮助: ./scripts/manage.sh help"