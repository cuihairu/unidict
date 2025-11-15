# UniDict Monorepo

一个现代化的多语言词典应用，采用Monorepo架构，集成AI翻译、语音评测、智能学习等功能。

## 🏗️ 项目架构

- **前端**: QML/Qt (桌面端), Flutter (移动端), React (Web端)
- **后端**: Go-Zero 微服务架构
- **数据库**: PostgreSQL + Redis + Elasticsearch
- **AI集成**: OpenAI, Claude, PaLM等多引擎支持

## 📁 目录结构

```
unidict/
├── 📱 apps/                  # 前端应用
├── 🚀 services/              # 后端微服务
├── 📚 packages/              # 共享库
├── 🛠️ tools/                # 开发工具
├── 📄 docs/                  # 文档
├── 🧪 tests/                # 测试
├── 🐳 deployments/          # 部署配置
├── 💾 data/                  # 数据文件
└── 📋 scripts/              # 脚本工具
```

## 🚀 快速开始

### 环境要求
- Go 1.21+
- Qt 6.0+
- Docker & Docker Compose
- Node.js 18+

### 本地开发
```bash
# 克隆项目
git clone <repo-url>
cd unidict

# 初始化项目
./scripts/setup.sh

# 启动服务
./scripts/dev.sh
```

### 构建项目
```bash
# 构建所有服务
./scripts/build.sh

# 构建特定服务
./scripts/build.sh --service=dictionary

# 构建桌面端
./scripts/build.sh --app=desktop
```

## 🔧 开发指南

### 服务开发
每个微服务使用go-zero框架，遵循标准目录结构：
- `cmd/`: 服务入口
- `internal/`: 私有代码
- `rpc/`: gRPC接口
- `etc/`: 配置文件

### 客户端开发
- 桌面端使用QML/Qt，位于`apps/desktop/`
- 移动端使用Flutter，位于`apps/mobile/`
- Web端使用React，位于`apps/web/`

### 共享库
- `packages/core/`: C++核心库
- `packages/shared-go/`: Go共享代码
- `packages/shared-types/`: 类型定义

## 📖 功能特性

### 🔍 核心功能
- [x] 海量词典资源 (70万+词条)
- [x] 全文搜索和模糊查询
- [x] 多格式词典支持
- [x] 离线查词功能

### 🤖 AI功能
- [x] 多引擎AI翻译
- [x] 智能写作辅助
- [x] 语法检查纠错
- [x] 内容扩写生成

### 📚 学习功能
- [x] 智能生词本
- [x] 记忆曲线复习
- [x] 学习进度追踪
- [x] 个性化学习计划

### 🎵 多媒体
- [x] 多音源发音
- [x] 语音评测打分
- [x] OCR文字识别
- [x] TTS语音合成

### 🌐 平台支持
- [x] Windows/Mac/Linux桌面端
- [x] iOS/Android移动端
- [x] Web浏览器端
- [x] 跨平台数据同步

## 🧪 测试

```bash
# 运行单元测试
./scripts/test.sh

# 运行集成测试
./scripts/test.sh --integration

# 运行E2E测试
./scripts/test.sh --e2e
```

## 🚀 部署

### 开发环境
```bash
docker-compose up -d
```

### 生产环境
```bash
# 使用Kubernetes
kubectl apply -f deployments/k8s/

# 使用Helm
helm install unidict deployments/helm/
```

## 📊 监控

- **指标监控**: Prometheus + Grafana
- **链路追踪**: Jaeger
- **日志收集**: ELK Stack
- **告警通知**: AlertManager

## 🤝 贡献指南

1. Fork项目
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送分支 (`git push origin feature/amazing-feature`)
5. 创建Pull Request

## 📄 许可证

本项目采用MIT许可证 - 详见[LICENSE](LICENSE)文件。

## 📞 联系我们

- 项目主页: <project-url>
- 问题反馈: <issues-url>
- 邮件联系: <email>

## 🙏 致谢

感谢所有贡献者和开源项目的支持！