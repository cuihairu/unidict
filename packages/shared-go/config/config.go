package config

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/spf13/viper"
	"github.com/zeromicro/go-zero/core/conf"
)

// Environment 环境类型
type Environment string

const (
	Development Environment = "development"
	Testing     Environment = "testing"
	Staging     Environment = "staging"
	Production  Environment = "production"
)

// DatabaseConfig 数据库配置
type DatabaseConfig struct {
	Host     string `json:"host" yaml:"host"`
	Port     int    `json:"port" yaml:"port"`
	Database string `json:"database" yaml:"database"`
	Username string `json:"username" yaml:"username"`
	Password string `json:"password" yaml:"password"`
	SSLMode  string `json:"ssl_mode" yaml:"ssl_mode"`
	MaxOpen  int    `json:"max_open" yaml:"max_open"`
	MaxIdle  int    `json:"max_idle" yaml:"max_idle"`
}

// RedisConfig Redis配置
type RedisConfig struct {
	Host     string `json:"host" yaml:"host"`
	Port     int    `json:"port" yaml:"port"`
	Password string `json:"password" yaml:"password"`
	Database int    `json:"database" yaml:"database"`
	PoolSize int    `json:"pool_size" yaml:"pool_size"`
}

// EtcdConfig 服务注册配置
type EtcdConfig struct {
	Endpoints []string `json:"endpoints" yaml:"endpoints"`
	Timeout   int      `json:"timeout" yaml:"timeout"`
}

// AIConfig AI服务配置
type AIConfig struct {
	OpenAI OpenAIConfig `json:"openai" yaml:"openai"`
	Claude ClaudeConfig `json:"claude" yaml:"claude"`
}

// OpenAIConfig OpenAI配置
type OpenAIConfig struct {
	APIKey  string `json:"api_key" yaml:"api_key"`
	BaseURL string `json:"base_url" yaml:"base_url"`
	Model   string `json:"model" yaml:"model"`
}

// ClaudeConfig Claude配置
type ClaudeConfig struct {
	APIKey  string `json:"api_key" yaml:"api_key"`
	BaseURL string `json:"base_url" yaml:"base_url"`
	Model   string `json:"model" yaml:"model"`
}

// ServiceConfig 通用服务配置
type ServiceConfig struct {
	Name        string          `json:"name" yaml:"name"`
	Environment Environment     `json:"environment" yaml:"environment"`
	Host        string          `json:"host" yaml:"host"`
	Port        int             `json:"port" yaml:"port"`
	Database    DatabaseConfig  `json:"database" yaml:"database"`
	Redis       RedisConfig     `json:"redis" yaml:"redis"`
	Etcd        EtcdConfig      `json:"etcd" yaml:"etcd"`
	AI          AIConfig        `json:"ai" yaml:"ai"`
	Log         LogConfig       `json:"log" yaml:"log"`
	JWT         JWTConfig       `json:"jwt" yaml:"jwt"`
}

// LogConfig 日志配置
type LogConfig struct {
	Level      string `json:"level" yaml:"level"`
	Format     string `json:"format" yaml:"format"`
	Output     string `json:"output" yaml:"output"`
	MaxSize    int    `json:"max_size" yaml:"max_size"`
	MaxBackups int    `json:"max_backups" yaml:"max_backups"`
	MaxAge     int    `json:"max_age" yaml:"max_age"`
}

// JWTConfig JWT配置
type JWTConfig struct {
	Secret     string `json:"secret" yaml:"secret"`
	ExpireTime int64  `json:"expire_time" yaml:"expire_time"`
	Issuer     string `json:"issuer" yaml:"issuer"`
}

// ConfigManager 配置管理器
type ConfigManager struct {
	viper *viper.Viper
	env   Environment
}

// NewConfigManager 创建配置管理器
func NewConfigManager() *ConfigManager {
	v := viper.New()

	// 设置配置文件搜索路径
	v.AddConfigPath(".")
	v.AddConfigPath("./etc")
	v.AddConfigPath("../etc")
	v.AddConfigPath("../../etc")
	v.AddConfigPath("/etc/unidict")

	// 设置环境变量
	v.AutomaticEnv()
	v.SetEnvPrefix("UNIDICT")
	v.SetEnvKeyReplacer(strings.NewReplacer(".", "_"))

	// 获取环境
	env := Environment(os.Getenv("UNIDICT_ENV"))
	if env == "" {
		env = Development
	}

	return &ConfigManager{
		viper: v,
		env:   env,
	}
}

// LoadConfig 加载配置
func (cm *ConfigManager) LoadConfig(configName string, config interface{}) error {
	// 设置配置文件名
	cm.viper.SetConfigName(configName)
	cm.viper.SetConfigType("yaml")

	// 尝试读取环境特定的配置
	envConfigName := fmt.Sprintf("%s-%s", configName, cm.env)
	cm.viper.SetConfigName(envConfigName)

	if err := cm.viper.ReadInConfig(); err != nil {
		// 如果环境特定配置不存在，尝试读取默认配置
		cm.viper.SetConfigName(configName)
		if err := cm.viper.ReadInConfig(); err != nil {
			return fmt.Errorf("读取配置文件失败: %w", err)
		}
	}

	// 解析配置
	if err := cm.viper.Unmarshal(config); err != nil {
		return fmt.Errorf("解析配置失败: %w", err)
	}

	return nil
}

// LoadZeroConfig 兼容go-zero的配置加载
func (cm *ConfigManager) LoadZeroConfig(configFile string, config interface{}) error {
	// 检查文件是否存在
	if _, err := os.Stat(configFile); os.IsNotExist(err) {
		// 尝试环境特定的配置文件
		dir := filepath.Dir(configFile)
		name := filepath.Base(configFile)
		ext := filepath.Ext(configFile)
		nameWithoutExt := strings.TrimSuffix(name, ext)

		envConfigFile := filepath.Join(dir, fmt.Sprintf("%s-%s%s", nameWithoutExt, cm.env, ext))
		if _, err := os.Stat(envConfigFile); err == nil {
			configFile = envConfigFile
		}
	}

	// 使用go-zero的配置加载
	return conf.Load(configFile, config)
}

// GetEnvironment 获取当前环境
func (cm *ConfigManager) GetEnvironment() Environment {
	return cm.env
}

// IsDevelopment 是否开发环境
func (cm *ConfigManager) IsDevelopment() bool {
	return cm.env == Development
}

// IsProduction 是否生产环境
func (cm *ConfigManager) IsProduction() bool {
	return cm.env == Production
}

// SetDefault 设置默认值
func (cm *ConfigManager) SetDefault(key string, value interface{}) {
	cm.viper.SetDefault(key, value)
}

// Get 获取配置值
func (cm *ConfigManager) Get(key string) interface{} {
	return cm.viper.Get(key)
}

// GetString 获取字符串配置
func (cm *ConfigManager) GetString(key string) string {
	return cm.viper.GetString(key)
}

// GetInt 获取整数配置
func (cm *ConfigManager) GetInt(key string) int {
	return cm.viper.GetInt(key)
}

// GetBool 获取布尔配置
func (cm *ConfigManager) GetBool(key string) bool {
	return cm.viper.GetBool(key)
}

// 全局配置管理器
var defaultConfigManager *ConfigManager

// Init 初始化全局配置管理器
func Init() {
	defaultConfigManager = NewConfigManager()
}

// LoadServiceConfig 加载服务配置的便捷方法
func LoadServiceConfig(configFile string, config interface{}) error {
	if defaultConfigManager == nil {
		Init()
	}
	return defaultConfigManager.LoadZeroConfig(configFile, config)
}

// GetCurrentEnvironment 获取当前环境
func GetCurrentEnvironment() Environment {
	if defaultConfigManager == nil {
		Init()
	}
	return defaultConfigManager.GetEnvironment()
}

// GetDSN 生成数据库连接字符串
func (dc *DatabaseConfig) GetDSN() string {
	return fmt.Sprintf("postgres://%s:%s@%s:%d/%s?sslmode=%s",
		dc.Username, dc.Password, dc.Host, dc.Port, dc.Database, dc.SSLMode)
}

// GetRedisAddr 生成Redis连接地址
func (rc *RedisConfig) GetRedisAddr() string {
	return fmt.Sprintf("%s:%d", rc.Host, rc.Port)
}