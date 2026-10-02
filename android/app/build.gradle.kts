// M1 冒烟壳。externalNativeBuild 指向仓库 adapters/android/CMakeLists.txt，
// 由 AGP 驱动 NDK 出 libunidict_jni.so（内链 unidict_std_core 静态库）打进 APK。
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")  // Compose 编译器（配对要求见根脚本注）
}

android {
    namespace = "dev.unidict.mobile"
    compileSdk = 36

    defaultConfig {
        applicationId = "dev.unidict.mobile"
        // minSdk 24 = M0 模拟器实测口径（std::filesystem/异常/STL 在 bionic 可用）
        minSdk = 24
        targetSdk = 36
        versionCode = 4
        versionName = "0.1.0-m5"
        // 锁 M0/M1 实测过的 NDK 版本：本机与 CI 同版，AGP 默认版本
        // runner 不一定预装，锁版避免触发整包下载
        ndkVersion = "27.0.12077973"
        ndk {
            // x86_64 供模拟器/CI 冒烟，arm64-v8a 供真机
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../adapters/android/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildFeatures {
        compose = true
    }

    // M5 打磨出包：release 开 R8（混淆+资源收缩，keep 见 proguard-rules.pro，
    // JNI 按名查类/构造器，缺 keep 即 NoSuchMethodError/UnsatisfiedLinkError）。
    // 签名：正式上架密钥待用户提供，暂用 debug 签名仅供 release 装机验证
    // （参照 cockpit M3 收口路径：先跑通 R8 后验证，再换正式签名）。
    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    sourceSets {
        getByName("main") {
            // 冒烟词典直接复用仓库唯一样张；dictionaries/ 是内置真实词典
            // CC-CEDICT 汉英（BUGS.md BUG-004 用户明令入库，CC BY-SA 4.0
            // 署名见 dictionaries/CC-CEDICT-ATTRIBUTION.md，覆盖"不新增
            // 词典资产"口径），首启种子进 APK 开箱即查
            assets.srcDir("../../examples")
            assets.srcDir("../../dictionaries")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.09.03"))
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    // View-based Material themes for manifest window background (cold start)
    implementation("com.google.android.material:material:1.12.0")
    // DictRepository 的 core 单线程执行器调度（asCoroutineDispatcher）
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")
}
