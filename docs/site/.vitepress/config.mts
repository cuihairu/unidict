import { defineConfig } from 'vitepress'

// 一站一主题：品牌色 #b11964（与桌面 QML/Android 壳同源），亮暗双套
// token 见 theme/custom.css；中文排版（首行缩进 2em/行高 1.75）也在那里。
// 部署在项目 Pages 子路径：https://cuihairu.github.io/unidict/
const base = '/unidict/'

// 页面清单：单一来源同时生成中英两套 nav/sidebar，避免中英镜像漂移。
// 站点布局为 fleet 约定：英文是默认 locale（root，源文件 X.md），
// 中文镜像在 zh/ 目录（源文件 zh/X.md），路由 /zh/<name>。
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
  return { text: en ? p.enText : p.zhText, link: en ? zh : `/zh${zh}` }
}

const group = (en: boolean, zhTitle: string, enTitle: string, items: string[]) => ({
  text: en ? enTitle : zhTitle,
  items: items.map((zh) => item(en, zh)),
})

const usagePages = ['/search', '/dictionaries', '/vocabulary', '/pronunciation', '/prototypes']
const advancedPages = ['/server', '/faq', '/research/dictionary-market-survey']

const nav = (en: boolean) => [
  { text: en ? 'Quick Start' : '快速上手', link: en ? '/quickstart' : '/zh/quickstart' },
  group(en, '使用', 'Usage', usagePages),
  group(en, '进阶', 'Advanced', advancedPages),
  { text: en ? 'Download' : '下载', link: en ? '/download' : '/zh/download' },
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
  lang: 'en-US',
  title: 'Unidict',
  description: 'An offline dictionary: many formats, many search modes, desktop and Android',
  base,
  // head 里的绝对路径不会被 VitePress 自动加 base 前缀，需自己拼
  head: [['link', { rel: 'icon', type: 'image/svg+xml', href: `${base}logo.svg` }]],

  // 语言栏：英文栏无前缀（默认栏，root），中文栏走 zh/ 目录。
  locales: {
    root: {
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
    zh: {
      label: '简体中文',
      lang: 'zh-CN',
      themeConfig: {
        ...uiLabels(false),
        nav: nav(false),
        sidebar: sidebar(false),
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
      },
    },
  },

  themeConfig: {
    // logo 走 themeConfig 的会被 VitePress 自动加 base，写原始路径即可
    logo: '/logo.svg',
    socialLinks: [{ icon: 'github', link: 'https://github.com/cuihairu/unidict' }],
    // 关掉主题的「对应页」跳转：它按 locales 目录形态推断对应路由，
    // 两侧源文件目录树同构（X.md 与 zh/X.md），推断应正确，但按页互链
    // 由每页顶部切换器承担，导航条语言下拉落到各自首页，保守关掉。
    i18nRouting: false,
  },
})
