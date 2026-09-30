// 版本矩阵照搬本机已验证先例（chirp apps/android）：Gradle 9.8 +
// AGP 9.x 内建 Kotlin（org.jetbrains.kotlin.android 已被 AGP9 拒收，
// .kt 随 sourceSets 直接编译，jvmTarget 跟 compileOptions）。
plugins {
    id("com.android.application") version "9.4.1" apply false
    // Compose 编译器插件：版本须与 AGP 内建 Kotlin 配对，不配对会在
    // 应用期报错——届时降级为经典 Views 冒烟页（见 app/build.gradle.kts 注）。
    id("org.jetbrains.kotlin.plugin.compose") version "2.2.20" apply false
}
