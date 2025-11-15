#!/bin/bash

# UniDict 项目管理脚本
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示帮助信息
show_help() {
    cat << EOF
UniDict 项目管理工具

用法: ./manage.sh [COMMAND] [OPTIONS]

命令:
  setup                初始化项目环境
  build                构建项目
  test                 运行测试
  dev                  启动开发环境
  deploy               部署应用
  clean                清理构建产物
  help                 显示此帮助信息

构建选项:
  --service=<name>     构建指定服务 (dictionary, user, etc.)
  --app=<name>         构建指定应用 (desktop, mobile, web)
  --all                构建所有组件

测试选项:
  --unit               运行单元测试
  --integration        运行集成测试
  --e2e                运行端到端测试

开发选项:
  --services           只启动后端服务
  --frontend           只启动前端应用
  --full               启动完整开发环境

部署选项:
  --env=<name>         部署环境 (dev, staging, prod)
  --service=<name>     部署指定服务

示例:
  ./manage.sh setup
  ./manage.sh build --service=dictionary
  ./manage.sh test --unit
  ./manage.sh dev --services
  ./manage.sh deploy --env=dev

EOF
}

# 检查依赖
check_dependencies() {
    log_info "检查项目依赖..."

    # 检查Go
    if ! command -v go &> /dev/null; then
        log_error "Go 未安装，请安装 Go 1.21 或更高版本"
        exit 1
    fi

    # 检查Docker
    if ! command -v docker &> /dev/null; then
        log_error "Docker 未安装"
        exit 1
    fi

    # 检查Docker Compose
    if ! command -v docker-compose &> /dev/null; then
        log_error "Docker Compose 未安装"
        exit 1
    fi

    # 检查Node.js
    if ! command -v node &> /dev/null; then
        log_warn "Node.js 未安装，Web应用构建将跳过"
    fi

    log_success "依赖检查完成"
}

# 初始化项目
setup_project() {
    log_info "初始化 UniDict 项目..."

    check_dependencies

    # 创建目录结构
    create_directory_structure

    # 安装Go依赖
    install_go_dependencies

    # 初始化数据库
    init_database

    # 生成配置文件
    generate_configs

    log_success "项目初始化完成"
}

# 创建目录结构
create_directory_structure() {
    log_info "创建目录结构..."

    mkdir -p "$PROJECT_ROOT"/{apps,services,packages,tools,docs,tests,deployments,data,scripts}
    mkdir -p "$PROJECT_ROOT/apps"/{desktop,mobile,web,admin}
    mkdir -p "$PROJECT_ROOT/packages"/{core,shared-go,shared-types,shared-config,shared-utils,shared-proto}
    mkdir -p "$PROJECT_ROOT/tools"/{build,deploy,migration,codegen}
    mkdir -p "$PROJECT_ROOT/tests"/{integration,e2e,load}
    mkdir -p "$PROJECT_ROOT/deployments"/{docker,k8s,helm}
    mkdir -p "$PROJECT_ROOT/data"/{dictionaries,samples,migrations}

    # 创建服务目录
    services=(gateway dictionary ai-translation ai-writing user learning media ocr tts speech document sync)
    for service in "${services[@]}"; do
        mkdir -p "$PROJECT_ROOT/services/$service"/{cmd,internal,rpc,etc}
        mkdir -p "$PROJECT_ROOT/services/$service/internal"/{config,handler,logic,model,svc}
    done

    log_success "目录结构创建完成"
}

# 安装Go依赖
install_go_dependencies() {
    log_info "安装Go依赖..."

    cd "$PROJECT_ROOT"

    # 初始化Go模块
    if [ ! -f "go.mod" ]; then
        go mod init github.com/unidict/unidict
    fi

    # 安装go-zero相关依赖
    go get -u github.com/zeromicro/go-zero
    go get -u github.com/zeromicro/go-zero/tools/goctl

    # 安装其他必要依赖
    go get -u github.com/golang/protobuf/protoc-gen-go
    go get -u google.golang.org/grpc/cmd/protoc-gen-go-grpc

    log_success "Go依赖安装完成"
}

# 初始化数据库
init_database() {
    log_info "初始化数据库..."

    # 启动数据库服务
    cd "$PROJECT_ROOT"
    docker-compose -f deployments/docker/docker-compose.dev.yml up -d postgres redis elasticsearch

    # 等待数据库启动
    sleep 10

    # 运行数据库迁移
    # TODO: 实现数据库迁移逻辑

    log_success "数据库初始化完成"
}

# 生成配置文件
generate_configs() {
    log_info "生成配置文件..."

    # TODO: 实现配置文件生成逻辑

    log_success "配置文件生成完成"
}

# 构建项目
build_project() {
    local service=""
    local app=""
    local build_all=false

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            --service=*)
                service="${1#*=}"
                shift
                ;;
            --app=*)
                app="${1#*=}"
                shift
                ;;
            --all)
                build_all=true
                shift
                ;;
            *)
                shift
                ;;
        esac
    done

    log_info "构建项目..."

    if [ "$build_all" = true ]; then
        build_all_services
        build_all_apps
    elif [ -n "$service" ]; then
        build_service "$service"
    elif [ -n "$app" ]; then
        build_app "$app"
    else
        build_all_services
    fi

    log_success "构建完成"
}

# 构建所有服务
build_all_services() {
    log_info "构建所有服务..."

    services=(gateway dictionary ai-translation ai-writing user learning media ocr tts speech document sync)
    for service in "${services[@]}"; do
        build_service "$service"
    done
}

# 构建指定服务
build_service() {
    local service=$1
    log_info "构建服务: $service"

    cd "$PROJECT_ROOT/services/$service"

    # 构建Go服务
    if [ -f "cmd/main.go" ]; then
        go build -o "bin/$service" cmd/main.go
    fi
}

# 构建所有应用
build_all_apps() {
    log_info "构建所有应用..."

    build_app "desktop"
    build_app "web"
    # build_app "mobile"  # 需要特殊的构建环境
}

# 构建指定应用
build_app() {
    local app=$1
    log_info "构建应用: $app"

    case $app in
        desktop)
            build_desktop_app
            ;;
        web)
            build_web_app
            ;;
        mobile)
            build_mobile_app
            ;;
        admin)
            build_admin_app
            ;;
        *)
            log_error "未知的应用: $app"
            exit 1
            ;;
    esac
}

# 构建桌面应用
build_desktop_app() {
    cd "$PROJECT_ROOT/apps/desktop"

    # 使用CMake构建
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc)
}

# 构建Web应用
build_web_app() {
    cd "$PROJECT_ROOT/apps/web"

    if [ -f "package.json" ]; then
        npm install
        npm run build
    fi
}

# 构建移动应用
build_mobile_app() {
    cd "$PROJECT_ROOT/apps/mobile"

    if [ -f "pubspec.yaml" ]; then
        flutter pub get
        flutter build apk
        flutter build ios
    fi
}

# 构建管理后台
build_admin_app() {
    cd "$PROJECT_ROOT/apps/admin"

    if [ -f "package.json" ]; then
        npm install
        npm run build
    fi
}

# 运行测试
run_tests() {
    local test_type=""

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            --unit)
                test_type="unit"
                shift
                ;;
            --integration)
                test_type="integration"
                shift
                ;;
            --e2e)
                test_type="e2e"
                shift
                ;;
            *)
                shift
                ;;
        esac
    done

    case $test_type in
        unit)
            run_unit_tests
            ;;
        integration)
            run_integration_tests
            ;;
        e2e)
            run_e2e_tests
            ;;
        *)
            run_all_tests
            ;;
    esac
}

# 运行单元测试
run_unit_tests() {
    log_info "运行单元测试..."

    cd "$PROJECT_ROOT"
    go test -v ./services/... -cover
}

# 运行集成测试
run_integration_tests() {
    log_info "运行集成测试..."

    # 启动测试环境
    cd "$PROJECT_ROOT"
    docker-compose -f deployments/docker/docker-compose.test.yml up -d

    # 等待服务启动
    sleep 30

    # 运行集成测试
    cd tests/integration
    go test -v ./...

    # 清理测试环境
    docker-compose -f deployments/docker/docker-compose.test.yml down
}

# 运行E2E测试
run_e2e_tests() {
    log_info "运行端到端测试..."

    cd "$PROJECT_ROOT/tests/e2e"
    # TODO: 实现E2E测试
}

# 运行所有测试
run_all_tests() {
    run_unit_tests
    run_integration_tests
    run_e2e_tests
}

# 启动开发环境
start_dev_environment() {
    local mode=""

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            --services)
                mode="services"
                shift
                ;;
            --frontend)
                mode="frontend"
                shift
                ;;
            --full)
                mode="full"
                shift
                ;;
            *)
                shift
                ;;
        esac
    done

    log_info "启动开发环境..."

    case $mode in
        services)
            start_backend_services
            ;;
        frontend)
            start_frontend_apps
            ;;
        *)
            start_full_environment
            ;;
    esac
}

# 启动后端服务
start_backend_services() {
    cd "$PROJECT_ROOT"
    docker-compose -f deployments/docker/docker-compose.dev.yml up -d
}

# 启动前端应用
start_frontend_apps() {
    # 启动Web应用
    cd "$PROJECT_ROOT/apps/web"
    if [ -f "package.json" ]; then
        npm start &
    fi

    # 启动桌面应用
    cd "$PROJECT_ROOT/apps/desktop"
    # TODO: 启动桌面应用
}

# 启动完整环境
start_full_environment() {
    start_backend_services
    sleep 30  # 等待后端服务启动
    start_frontend_apps
}

# 部署应用
deploy_application() {
    local env=""
    local service=""

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            --env=*)
                env="${1#*=}"
                shift
                ;;
            --service=*)
                service="${1#*=}"
                shift
                ;;
            *)
                shift
                ;;
        esac
    done

    log_info "部署应用到环境: $env"

    case $env in
        dev)
            deploy_to_dev "$service"
            ;;
        staging)
            deploy_to_staging "$service"
            ;;
        prod)
            deploy_to_production "$service"
            ;;
        *)
            log_error "未知的部署环境: $env"
            exit 1
            ;;
    esac
}

# 部署到开发环境
deploy_to_dev() {
    local service=$1

    cd "$PROJECT_ROOT"
    docker-compose -f deployments/docker/docker-compose.dev.yml up -d
}

# 部署到测试环境
deploy_to_staging() {
    local service=$1

    # TODO: 实现测试环境部署逻辑
    kubectl apply -f deployments/k8s/staging/
}

# 部署到生产环境
deploy_to_production() {
    local service=$1

    # TODO: 实现生产环境部署逻辑
    helm upgrade --install unidict deployments/helm/ --namespace unidict-prod
}

# 清理构建产物
clean_project() {
    log_info "清理构建产物..."

    # 清理Go构建产物
    find "$PROJECT_ROOT" -name "bin" -type d -exec rm -rf {} + 2>/dev/null || true
    find "$PROJECT_ROOT" -name "*.exe" -type f -delete 2>/dev/null || true

    # 清理CMake构建产物
    find "$PROJECT_ROOT" -name "build" -type d -exec rm -rf {} + 2>/dev/null || true

    # 清理Node.js构建产物
    find "$PROJECT_ROOT" -name "node_modules" -type d -exec rm -rf {} + 2>/dev/null || true
    find "$PROJECT_ROOT" -name "dist" -type d -exec rm -rf {} + 2>/dev/null || true

    log_success "清理完成"
}

# 主函数
main() {
    case ${1:-help} in
        setup)
            setup_project
            ;;
        build)
            shift
            build_project "$@"
            ;;
        test)
            shift
            run_tests "$@"
            ;;
        dev)
            shift
            start_dev_environment "$@"
            ;;
        deploy)
            shift
            deploy_application "$@"
            ;;
        clean)
            clean_project
            ;;
        help|*)
            show_help
            ;;
    esac
}

# 运行主函数
main "$@"