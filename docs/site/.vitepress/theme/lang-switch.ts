// 站点的语言面：首访按浏览器语言决定一次，显式选择永久记住。
//
// 两栏路由由 locales 承担（root = 英文，`zh/` = 中文），`i18nRouting` 为 false，
// 所以 VitePress 自己不会换路由，每页顶部的切换行是唯一入口。这里补两件它没做的事：
// 1. 首访（从未记录过选择）按 `navigator.languages` 检测一次，跳去对应那一栏；
// 2. 用户点过语言入口就把选择写进 localStorage，之后检测与跳转都让位。
//
// 只有两种点击算「表态」：本页另一栏的同名页（每页顶部的切换行），以及两个栏首页。
// 普通侧栏/正文链接一律不算，不会把正常阅读误记成语言选择。

const KEY = 'unidict-doc-lang'
const base = import.meta.env.BASE_URL

type Side = 'en' | 'zh'

// 剥掉站点 base 与尾斜杠，拿到相对路径（'architecture' / 'zh/architecture' / '' / 'zh'）。
// 尾斜杠必须归一化：栏首页 href 是 `zh/`，不归一化会和裸 `zh` 判成两种形态。
function relOf(path: string): string {
  const rel = path.startsWith(base) ? path.slice(base.length) : path.replace(/^\//, '')
  return rel.replace(/\/$/, '')
}

function sideOf(path: string): Side {
  const rel = relOf(path)
  return rel === 'zh' || rel.startsWith('zh/') ? 'zh' : 'en'
}

// 另一栏的同名页：`zh/` 前缀加上或剥掉，其余原样。
function counterpart(path: string): string {
  const rel = relOf(path)
  const rest = rel === 'zh' ? '' : rel.startsWith('zh/') ? rel.slice(3) : rel
  const next = sideOf(path) === 'en' ? `zh/${rest}` : rest
  return `${base}${next}`
}

function readChoice(): Side | null {
  try {
    const v = window.localStorage.getItem(KEY)
    return v === 'en' || v === 'zh' ? v : null
  } catch {
    return null
  }
}

function writeChoice(choice: Side): void {
  try {
    window.localStorage.setItem(KEY, choice)
  } catch {
    // 隐私模式等写不进：本次会话仍可跳转，只是下次再来一次检测
  }
}

function detect(): Side {
  const langs = window.navigator.languages?.length ? window.navigator.languages : [window.navigator.language]
  return langs.some((l) => l.toLowerCase().startsWith('zh')) ? 'zh' : 'en'
}

function captureClick(event: MouseEvent): void {
  const anchor = (event.target as Element | null)?.closest?.('a[href]') as HTMLAnchorElement | null
  const href = anchor?.getAttribute('href')
  if (!href) return
  // 只认站内链接：外链、mailto、同页锚点都不算语言表态
  if (/^(https?:|mailto:|#)/i.test(href)) return
  if (anchor?.target === '_blank' || anchor?.rel?.includes('external')) return

  if (href === counterpart(window.location.pathname)) writeChoice(sideOf(href))
  else if (relOf(href) === '') writeChoice('en')
  else if (relOf(href) === 'zh') writeChoice('zh')
}

export function installLangSwitch(): void {
  if (typeof window === 'undefined') return

  if (readChoice() === null) {
    const want = detect()
    if (want !== sideOf(window.location.pathname)) {
      try {
        // 本次会话只跳一次，存储写不进也不会自跳成环
        window.sessionStorage.setItem(`${KEY}:tried`, '1')
      } catch {
        // 忽略
      }
      window.location.replace(counterpart(window.location.pathname))
      return
    }
  }

  document.addEventListener('click', captureClick, true)
}