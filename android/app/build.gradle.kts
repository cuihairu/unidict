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
        versionCode = 3
        versionName = "0.1.0-m4"
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

    sourceSets {
        getByName("main") {
            // 冒烟词典直接复用仓库唯一样张（AGENTS.md：不新增词典资产）
            assets.srcDir("../../examples")
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
    // DictRepository 的 core 单线程执行器调度（asCoroutineDispatcher）
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")
}
