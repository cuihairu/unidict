package utils

import (
	"crypto/md5"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"reflect"
	"regexp"
	"strconv"
	"strings"
	"time"
	"unicode"

	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"
)

// StringUtils 字符串工具函数

// IsEmpty 检查字符串是否为空
func IsEmpty(s string) bool {
	return len(strings.TrimSpace(s)) == 0
}

// IsNotEmpty 检查字符串是否不为空
func IsNotEmpty(s string) bool {
	return !IsEmpty(s)
}

// DefaultString 返回默认字符串值
func DefaultString(s, defaultValue string) string {
	if IsEmpty(s) {
		return defaultValue
	}
	return s
}

// MD5 计算字符串的MD5值
func MD5(s string) string {
	h := md5.New()
	h.Write([]byte(s))
	return hex.EncodeToString(h.Sum(nil))
}

// SHA256 计算字符串的SHA256值
func SHA256(s string) string {
	h := sha256.New()
	h.Write([]byte(s))
	return hex.EncodeToString(h.Sum(nil))
}

// GenerateRandomString 生成指定长度的随机字符串
func GenerateRandomString(length int) string {
	const charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	b := make([]byte, length)
	rand.Read(b)
	for i := range b {
		b[i] = charset[b[i]%byte(len(charset))]
	}
	return string(b)
}

// GenerateUUID 生成UUID
func GenerateUUID() string {
	return uuid.New().String()
}

// PasswordUtils 密码工具函数

// HashPassword 使用bcrypt加密密码
func HashPassword(password string) (string, error) {
	bytes, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	return string(bytes), err
}

// CheckPassword 验证密码
func CheckPassword(password, hashedPassword string) bool {
	err := bcrypt.CompareHashAndPassword([]byte(hashedPassword), []byte(password))
	return err == nil
}

// ValidatePassword 验证密码强度
func ValidatePassword(password string) error {
	if len(password) < 8 {
		return fmt.Errorf("密码长度至少8位")
	}

	hasUpper := false
	hasLower := false
	hasDigit := false
	hasSpecial := false

	for _, char := range password {
		switch {
		case unicode.IsUpper(char):
			hasUpper = true
		case unicode.IsLower(char):
			hasLower = true
		case unicode.IsDigit(char):
			hasDigit = true
		case unicode.IsPunct(char) || unicode.IsSymbol(char):
			hasSpecial = true
		}
	}

	if !hasUpper {
		return fmt.Errorf("密码必须包含大写字母")
	}
	if !hasLower {
		return fmt.Errorf("密码必须包含小写字母")
	}
	if !hasDigit {
		return fmt.Errorf("密码必须包含数字")
	}
	if !hasSpecial {
		return fmt.Errorf("密码必须包含特殊字符")
	}

	return nil
}

// ValidationUtils 验证工具函数

// IsEmail 验证邮箱格式
func IsEmail(email string) bool {
	pattern := `^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$`
	matched, _ := regexp.MatchString(pattern, email)
	return matched
}

// IsPhone 验证手机号格式（支持中国手机号）
func IsPhone(phone string) bool {
	pattern := `^1[3-9]\d{9}$`
	matched, _ := regexp.MatchString(pattern, phone)
	return matched
}

// IsURL 验证URL格式
func IsURL(url string) bool {
	pattern := `^https?://[^\s/$.?#].[^\s]*$`
	matched, _ := regexp.MatchString(pattern, url)
	return matched
}

// ConvertUtils 类型转换工具函数

// ToString 转换为字符串
func ToString(value interface{}) string {
	if value == nil {
		return ""
	}

	switch v := value.(type) {
	case string:
		return v
	case int, int8, int16, int32, int64:
		return fmt.Sprintf("%d", v)
	case uint, uint8, uint16, uint32, uint64:
		return fmt.Sprintf("%d", v)
	case float32, float64:
		return fmt.Sprintf("%f", v)
	case bool:
		return strconv.FormatBool(v)
	case time.Time:
		return v.Format(time.RFC3339)
	default:
		return fmt.Sprintf("%v", v)
	}
}

// ToInt 转换为整数
func ToInt(value interface{}) (int, error) {
	switch v := value.(type) {
	case int:
		return v, nil
	case int64:
		return int(v), nil
	case float64:
		return int(v), nil
	case string:
		return strconv.Atoi(v)
	default:
		return 0, fmt.Errorf("无法转换为整数: %v", value)
	}
}

// ToFloat64 转换为浮点数
func ToFloat64(value interface{}) (float64, error) {
	switch v := value.(type) {
	case float64:
		return v, nil
	case float32:
		return float64(v), nil
	case int:
		return float64(v), nil
	case string:
		return strconv.ParseFloat(v, 64)
	default:
		return 0, fmt.Errorf("无法转换为浮点数: %v", value)
	}
}

// ToBool 转换为布尔值
func ToBool(value interface{}) (bool, error) {
	switch v := value.(type) {
	case bool:
		return v, nil
	case string:
		return strconv.ParseBool(v)
	case int:
		return v != 0, nil
	default:
		return false, fmt.Errorf("无法转换为布尔值: %v", value)
	}
}

// SliceUtils 切片工具函数

// Contains 检查切片是否包含指定元素
func Contains[T comparable](slice []T, item T) bool {
	for _, s := range slice {
		if s == item {
			return true
		}
	}
	return false
}

// Remove 从切片中移除指定元素
func Remove[T comparable](slice []T, item T) []T {
	var result []T
	for _, s := range slice {
		if s != item {
			result = append(result, s)
		}
	}
	return result
}

// Unique 去除切片中的重复元素
func Unique[T comparable](slice []T) []T {
	keys := make(map[T]bool)
	var result []T

	for _, item := range slice {
		if !keys[item] {
			keys[item] = true
			result = append(result, item)
		}
	}
	return result
}

// MapUtils Map工具函数

// GetMapValue 安全获取map值
func GetMapValue[K comparable, V any](m map[K]V, key K, defaultValue V) V {
	if value, ok := m[key]; ok {
		return value
	}
	return defaultValue
}

// MergeMap 合并多个map
func MergeMap[K comparable, V any](maps ...map[K]V) map[K]V {
	result := make(map[K]V)
	for _, m := range maps {
		for k, v := range m {
			result[k] = v
		}
	}
	return result
}

// TimeUtils 时间工具函数

// FormatTime 格式化时间
func FormatTime(t time.Time, layout string) string {
	if layout == "" {
		layout = "2006-01-02 15:04:05"
	}
	return t.Format(layout)
}

// ParseTime 解析时间字符串
func ParseTime(timeStr, layout string) (time.Time, error) {
	if layout == "" {
		layout = "2006-01-02 15:04:05"
	}
	return time.Parse(layout, timeStr)
}

// GetStartOfDay 获取当天开始时间
func GetStartOfDay(t time.Time) time.Time {
	return time.Date(t.Year(), t.Month(), t.Day(), 0, 0, 0, 0, t.Location())
}

// GetEndOfDay 获取当天结束时间
func GetEndOfDay(t time.Time) time.Time {
	return time.Date(t.Year(), t.Month(), t.Day(), 23, 59, 59, 999999999, t.Location())
}

// ReflectUtils 反射工具函数

// IsNil 检查interface{}是否为nil
func IsNil(v interface{}) bool {
	if v == nil {
		return true
	}
	rv := reflect.ValueOf(v)
	return rv.Kind() == reflect.Ptr && rv.IsNil()
}

// GetTypeName 获取类型名称
func GetTypeName(v interface{}) string {
	return reflect.TypeOf(v).String()
}

// StructToMap 结构体转map
func StructToMap(obj interface{}) map[string]interface{} {
	result := make(map[string]interface{})
	v := reflect.ValueOf(obj)
	t := reflect.TypeOf(obj)

	if v.Kind() == reflect.Ptr {
		v = v.Elem()
		t = t.Elem()
	}

	if v.Kind() != reflect.Struct {
		return result
	}

	for i := 0; i < v.NumField(); i++ {
		field := t.Field(i)
		value := v.Field(i)

		if field.IsExported() {
			result[field.Name] = value.Interface()
		}
	}

	return result
}

// ErrorUtils 错误处理工具函数

// PanicRecover 恢复panic并返回错误
func PanicRecover() error {
	if r := recover(); r != nil {
		return fmt.Errorf("panic recovered: %v", r)
	}
	return nil
}

// WrapError 包装错误信息
func WrapError(err error, message string) error {
	if err == nil {
		return nil
	}
	return fmt.Errorf("%s: %w", message, err)
}