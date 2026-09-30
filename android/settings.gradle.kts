// unidict Android 壳（mobile_plan 方案 B：Kotlin 原生壳 + NDK std-only core）。
// JNI 胶水在仓库 adapters/android/（经 app 的 externalNativeBuild 进包），
// core 静态库由同一 CMake 工程产出——仓库单一事实源，不在壳内复制代码。
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "unidict-android"
include(":app")
