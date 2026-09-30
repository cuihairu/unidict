package dev.unidict.mobile

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

// 品牌主题——数值与桌面端 qmlui/theme_tokens.h 同源同观感（WCAG AA 实测
// 全 ≥4.5:1，见该头注释与 tests/theme_tokens_contrast_test.cpp）：
//   品牌 #b11964（docs/logo.svg 全库唯一色）衍生的 accent 系 +
//   带同色相玫瑰调的中性系。形状三档 小件4/控件8/卡片12 对齐 kRadiusS/M/L。
// Material3 角色映射：primary=accent、error=danger、tertiary=success、
// secondary 文字两级映射 textSecondary/textTertiary；桌面无对应的
// container 系按同色相派生（仅容器底，正文对比度角色不落在其上）。

private val LightColors: ColorScheme = lightColorScheme(
    primary = Color(0xFFb11964),
    onPrimary = Color(0xFFffffff),
    primaryContainer = Color(0xFFffd9e6),
    onPrimaryContainer = Color(0xFF3f001d),
    inversePrimary = Color(0xFFff6fa9),
    secondary = Color(0xFF6d5f67),
    onSecondary = Color(0xFFffffff),
    secondaryContainer = Color(0xFFf3e9ee),
    onSecondaryContainer = Color(0xFF241a20),
    tertiary = Color(0xFF2e7d32),        // success
    onTertiary = Color(0xFFffffff),
    tertiaryContainer = Color(0xFFd9f2dc),
    onTertiaryContainer = Color(0xFF0d2810),
    background = Color(0xFFfaf7f9),
    onBackground = Color(0xFF241a20),
    surface = Color(0xFFffffff),
    onSurface = Color(0xFF241a20),
    surfaceVariant = Color(0xFFf3e9ee),
    onSurfaceVariant = Color(0xFF6d5f67),
    outline = Color(0xFFb7aab1),         // textDisabled 档（描边非正文）
    outlineVariant = Color(0xFFeadfe5),  // divider
    error = Color(0xFFc42b1c),
    onError = Color(0xFFffffff),
    errorContainer = Color(0xFFfbdad5),
    onErrorContainer = Color(0xFF3b0703),
    surfaceTint = Color(0xFFb11964),
    inverseSurface = Color(0xFF241a20),
    inverseOnSurface = Color(0xFFfaf7f9),
    scrim = Color(0xFF000000),
)

private val DarkColors: ColorScheme = darkColorScheme(
    primary = Color(0xFFff6fa9),
    onPrimary = Color(0xFF2b0a1a),
    primaryContainer = Color(0xFF5c0e37),
    onPrimaryContainer = Color(0xFFffd9e6),
    inversePrimary = Color(0xFFb11964),
    secondary = Color(0xFFb6a7af),
    onSecondary = Color(0xFF2b1a23),
    secondaryContainer = Color(0xFF3b2f35),
    onSecondaryContainer = Color(0xFFf2eaef),
    tertiary = Color(0xFF7ddc8a),        // success
    onTertiary = Color(0xFF0d2810),
    tertiaryContainer = Color(0xFF1e4623),
    onTertiaryContainer = Color(0xFFd9f2dc),
    background = Color(0xFF1a1417),
    onBackground = Color(0xFFf2eaef),
    surface = Color(0xFF251c21),
    onSurface = Color(0xFFf2eaef),
    surfaceVariant = Color(0xFF3b2f35),
    onSurfaceVariant = Color(0xFFb6a7af),
    outline = Color(0xFF67585f),
    outlineVariant = Color(0xFF3b2f35),
    error = Color(0xFFff8a7a),
    onError = Color(0xFF3b0703),
    errorContainer = Color(0xFF5c1710),
    onErrorContainer = Color(0xFFfbdad5),
    surfaceTint = Color(0xFFff6fa9),
    inverseSurface = Color(0xFFf2eaef),
    inverseOnSurface = Color(0xFF1a1417),
    scrim = Color(0xFF000000),
)

// 形状三档：小件 4 / 控件 8 / 卡片 12（与桌面 kRadiusS/M/L 同档）
private val UnidictShapes = Shapes(
    extraSmall = RoundedCornerShape(4.dp),
    small = RoundedCornerShape(4.dp),
    medium = RoundedCornerShape(8.dp),
    large = RoundedCornerShape(12.dp),
    extraLarge = RoundedCornerShape(12.dp),
)

@Composable
fun UnidictTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    MaterialTheme(
        colorScheme = if (darkTheme) DarkColors else LightColors,
        shapes = UnidictShapes,
        content = content,
    )
}
