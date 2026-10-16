import { defineConfig } from 'vitepress'

// 一站一主题：品牌色 #b11964（与桌面 QML/Android 壳同源），亮暗双套
// token 见 theme/custom.css；中文排版（首行缩进 2em/行高 1.75）也在那里。
// 部署在项目 Pages 子路径：https://cuihairu.github.io/unidict/
const base = '/unidict/'

// 页面清单：单一来源同时生成中英两套 nav/sidebar，避免中英镜像漂移。
// zh 是中文路由，英文路由统一是 /en + zh（源文件映射见下面的 rewrites）。
const pages = [
  { zh: '/quickstart', zhText: '快速上手', enText: 'Quick Start' },
  { zh: '/search', zhText: '查词', enText: 'Looking Up Words' },
  { zh: '/dictionaries', zhText: '词典管理', enText: 'Dictionary Management' },
  { zh: '/vocabulary', zhText: '生词本', enText: 'Vocabulary Book' },
  { zh: '/pronunciation', zhText: '发音', enText: 'Pronunciation' },
  { zh: '/prototypes', zhText: '界面原型', enText: 'UI Prototypes' },
  { zh: '/server', zhText: '服务端规划', enText: 'Server Roadmap' },
  { zh: '/faq', zhText: '常见问题', enText: 'FAQ' },
  { zh: '/research/dictionary-market-survey', zhText: '市场调研', enText: 'Market Survey' },
]

const item = (en: boolean, zh: string): { text: string; link: string } => {
  const p = pages.find((x) => x.zh === zh)!
  return { text: en ? p.enText : p.zhText, link: en ? `/en${zh}` : zh }
}

const group = (en: boolean, zhTitle: string, enTitle: string, items: string[]) => ({
  text: en ? enTitle : zhTitle,
  items: items.map((zh) => item(en, zh)),
})

const usagePages = ['/search', '/dictionaries', '/vocabulary', '/pronunciation', '/prototypes']
const advancedPages = ['/server', '/faq', '/research/dictionary-market-survey']

const nav = (en: boolean) => [
  { text: en ? 'Quick Start' : '快速上手', link: en ? '/en/quickstart' : '/quickstart' },
  group(en, '使用', 'Usage', usagePages),
  group(en, '进阶', 'Advanced', advancedPages),
  { text: en ? 'Download' : '下载', link: en ? '/en/download' : '/download' },
]

const sidebar = (en: boolean) => [
  group(en, '开始', 'Getting Started', ['/quickstart']),
  group(en, '使用', 'Usage', usagePages),
  group(en, '进阶', 'Advanced', advancedPages),
]

// 逐栏界面文案：VitePress 的 themeConfig 合并是浅合并，两侧都要整键给出。
const uiLabels = (en: boolean) => en
  ? {
      siteTitle: 'Unidict Docs',
      outline: { level: [2, 3], label: 'On this page' },
      docFooter: { prev: 'Previous', next: 'Next' },
      lastUpdated: { text: 'Last updated' },
      darkModeSwitchLabel: 'Theme',
      returnToTopLabel: 'Back to top',
      sidebarMenuLabel: 'Contents',
    }
  : {
      siteTitle: 'Unidict 文档',
      outline: { level: [2, 3], label: '本页' },
      docFooter: { prev: '上一篇', next: '下一篇' },
      lastUpdated: { text: '更新于' },
      darkModeSwitchLabel: '主题',
      returnToTopLabel: '回到顶部',
      sidebarMenuLabel: '目录',
    }

export default defineConfig({
  lang: 'zh-CN',
  title: 'Unidict',
  description: '离线词典：多格式词典、多模式检索、桌面与 Android 双端',
  base,
  // 英文源文件保持 <name>.en.md 与中文页同目录配对（顶部互链），
  // 但路由映射到 en/<name>.md：VitePress 的 locales 靠路由前缀识别语言，
  // 英文页由此按页拿到 en-US 的 lang 声明、英文 nav/sidebar 与界面文案。
  rewrites: (page) => (page.endsWith('.en.md') ? `en/${page.replace(/\.en\.md$/, '.md')}` : page),
  // head 里的绝对路径不会被 VitePress 自动加 base 前缀，需自己拼
  head: [['link', { rel: 'icon', type: 'image/svg+xml', href: `${base}logo.svg` }]],

  // 语言栏：中文栏无前缀（默认栏），英文栏走 en/ 目录。
  locales: {
    root: { label: '简体中文' },
    en: {
      label: 'English',
      lang: 'en-US',
      description: 'An offline dictionary: many formats, many search modes, desktop and Android',
      themeConfig: {
        ...uiLabels(true),
        nav: nav(true),
        sidebar: sidebar(true),
        // search 是整键替换，英文栏单独声明以拿英文默认文案
        search: { provider: 'local' },
      },
    },
  },

  themeConfig: {
    ...uiLabels(false),
    // logo 走 themeConfig 的会被 VitePress 自动加 base，写原始路径即可
    logo: '/logo.svg',
    nav: nav(false),
    sidebar: sidebar(false),
    socialLinks: [{ icon: 'github', link: 'https://github.com/cuihairu/unidict' }],
    search: {
      provider: 'local',
      options: {
        translations: {
          button: { buttonText: '搜索文档', buttonAriaLabel: '搜索文档' },
          modal: {
            noResultsText: '没有结果',
            resetButtonTitle: '清空条件',
            footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' },
          },
        },
      },
    },
    // 关掉主题的「对应页」跳转：它按 locales 目录形态推断对应路由，
    // 而源文件是 <name>.en.md 平铺配对，算出的链接会错。
    // 按页互链由每页顶部切换器承担，导航条语言下拉落到各自首页。
    i18nRouting: false,
  },
})
