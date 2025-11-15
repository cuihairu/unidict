module github.com/unidict/unidict

go 1.21

require (
	github.com/zeromicro/go-zero v1.5.6
	google.golang.org/grpc v1.59.0
	google.golang.org/protobuf v1.31.0
	github.com/golang/protobuf v1.5.3
)

require (
	// 数据库驱动
	github.com/lib/pq v1.10.9
	github.com/go-redis/redis/v8 v8.11.5
	github.com/olivere/elastic/v7 v7.0.32

	// AI服务集成
	github.com/sashabaranov/go-openai v1.17.9
	github.com/anthropics/anthropic-sdk-go v0.2.0-alpha.1

	// 图像处理和OCR
	github.com/otiai10/gosseract/v2 v2.4.1
	github.com/disintegration/imaging v1.6.2

	// 音频处理
	github.com/hajimehoshi/go-mp3 v0.3.4
	github.com/hajimehoshi/oto/v2 v2.4.2

	// 配置管理
	github.com/spf13/viper v1.17.0
	github.com/spf13/cobra v1.8.0

	// 日志和监控
	github.com/sirupsen/logrus v1.9.3
	github.com/prometheus/client_golang v1.17.0
	github.com/opentracing/opentracing-go v1.2.0
	github.com/uber/jaeger-client-go v2.30.0+incompatible

	// 工具库
	github.com/google/uuid v1.4.0
	github.com/golang-jwt/jwt/v5 v5.2.0
	github.com/pkg/errors v0.9.1
	golang.org/x/crypto v0.15.0
	golang.org/x/sync v0.5.0
	golang.org/x/time v0.5.0

	// 测试框架
	github.com/stretchr/testify v1.8.4
	github.com/golang/mock v1.6.0

	// 文档处理
	github.com/unidoc/unioffice v1.25.0
	github.com/ledongthuc/pdf v0.0.0-20220302134840-0c2507a12d80

	// 消息队列
	github.com/Shopify/sarama v1.37.2
	github.com/IBM/sarama v1.42.1

	// 对象存储
	github.com/minio/minio-go/v7 v7.0.66
)

// 本地包替换 (开发期间)
replace (
	github.com/unidict/unidict/packages/shared-go => ./packages/shared-go
	github.com/unidict/unidict/packages/shared-types => ./packages/shared-types
	github.com/unidict/unidict/packages/shared-config => ./packages/shared-config
	github.com/unidict/unidict/packages/shared-utils => ./packages/shared-utils
)