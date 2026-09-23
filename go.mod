module github.com/unidict/unidict

go 1.21

require (
	github.com/golang/protobuf v1.5.4
	github.com/zeromicro/go-zero v1.5.6
	google.golang.org/grpc v1.64.1
	google.golang.org/protobuf v1.33.0
)

require (
	github.com/IBM/sarama v1.42.1

	// 消息队列
	github.com/Shopify/sarama v1.37.2
	github.com/disintegration/imaging v1.6.2
	github.com/go-redis/redis/v8 v8.11.5
	github.com/golang-jwt/jwt/v5 v5.2.2
	github.com/golang/mock v1.6.0

	// 工具库
	github.com/google/uuid v1.6.0

	// 音频处理
	github.com/hajimehoshi/go-mp3 v0.3.4
	github.com/hajimehoshi/oto/v2 v2.4.2
	github.com/ledongthuc/pdf v0.0.0-20220302134840-0c2507a12d80
	// 数据库驱动
	github.com/lib/pq v1.10.9

	// 对象存储
	github.com/minio/minio-go/v7 v7.0.66
	github.com/olivere/elastic/v7 v7.0.32
	github.com/opentracing/opentracing-go v1.2.0

	// 图像处理和OCR
	github.com/otiai10/gosseract/v2 v2.4.1
	github.com/pkg/errors v0.9.1
	github.com/prometheus/client_golang v1.17.0

	// AI服务集成
	github.com/sashabaranov/go-openai v1.17.9

	// 日志和监控
	github.com/sirupsen/logrus v1.9.3
	github.com/spf13/cobra v1.8.0

	// 配置管理
	github.com/spf13/viper v1.17.0

	// 测试框架
	github.com/stretchr/testify v1.8.4
	github.com/uber/jaeger-client-go v2.30.0+incompatible

	// 文档处理
	github.com/unidoc/unioffice v1.25.0
	golang.org/x/crypto v0.25.0
	golang.org/x/sync v0.7.0
	golang.org/x/time v0.5.0
)

// 本地包替换 (开发期间)
replace (
	github.com/unidict/unidict/packages/shared-config => ./packages/shared-config
	github.com/unidict/unidict/packages/shared-go => ./packages/shared-go
	github.com/unidict/unidict/packages/shared-types => ./packages/shared-types
	github.com/unidict/unidict/packages/shared-utils => ./packages/shared-utils
)
