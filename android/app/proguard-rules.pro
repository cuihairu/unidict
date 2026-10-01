# M5 release：R8/资源收缩 keep 清单（逐项注明原因，参照 cockpit M3 口径）。
# JNI 按名查类/查构造器（adapters/android/jni/jni_util.h + 各 *_jni.cpp）：
#   FindClass("dev/unidict/mobile/SearchHit|VocabItem|DictMetaInfo")
#   + GetMethodID("<init>", "(Ljava/lang/String;…​)V")——混淆改名即
#   NoSuchMethodError，release 必崩，故类名 + 构造器签名必须保留。

# native 方法按名注册（Java_dev_unidict_mobile_UnidictCore_*）：类名与
# 方法名任一被改即 UnsatisfiedLinkError。
-keep class dev.unidict.mobile.UnidictCore { *; }

# C++ 侧 NewObject 逐条构造的三个数据类：保类名 + 构造器（字段经构造器
# 一次性传入，Compose 侧 getter 直调由 R8 正常处理，无需额外 keep）。
-keep class dev.unidict.mobile.SearchHit {
    public <init>(java.lang.String, java.lang.String, java.lang.String);
}
-keep class dev.unidict.mobile.VocabItem {
    public <init>(java.lang.String, java.lang.String, java.lang.String, java.lang.String);
}
-keep class dev.unidict.mobile.DictMetaInfo {
    public <init>(java.lang.String, int, java.lang.String);
}
