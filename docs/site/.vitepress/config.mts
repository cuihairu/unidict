import { defineConfig } from 'vitepress'

// 一站一主题：品牌色 #b11964（与桌面 QML/Android 壳同源），亮暗双套
// token 见 theme/custom.css；中文排版（首行缩进 2em/行高 1.75）也在那里。
export default defineConfig({
  lang: 'zh-CN',
  title: 'Unidict',
  description: '离线词典工作台：多格式词典、多模式检索、桌面与 Android 双端',
  head: [['link', { rel: 'icon', type: 'image/svg+xml', href: '/logo.svg' }]],

  themeConfig: {
    logo: '/logo.svg',
    siteTitle: 'Unidict 文档',
    nav: [
      { text: '开始', items: [
        { text: '快速上手', link: '/quickstart' },
        { text: '下载与安装', link: '/install' },
      ]},
      { text: '使用', items: [
        { text: '查词', link: '/search' },
        { text: '词典管理', link: '/dictionaries' },
        { text: '生词本', link: '/vocabulary' },
        { text: '发音', link: '/pronunciation' },
      ]},
      { text: '进阶', items: [
        { text: '服务端规划', link: '/server' },
        { text: '常见问题', link: '/faq' },
      ]},
      { text: '下载', link: '/download' },
    ],
    sidebar: [
      {
        text: '开始',
        items: [
          { text: '快速上手', link: '/quickstart' },
          { text: '下载与安装', link: '/install' },
        ],
      },
      {
        text: '使用',
        items: [
          { text: '查词', link: '/search' },
          { text: '词典管理', link: '/dictionaries' },
          { text: '生词本', link: '/vocabulary' },
          { text: '发音', link: '/pronunciation' },
        ],
      },
      {
        text: '进阶',
        items: [
          { text: '服务端规划', link: '/server' },
          { text: '常见问题', link: '/faq' },
        ],
      },
    ],
    socialLinks: [{ icon: 'github', link: 'https://github.com/cuihairu/unidict' }],
    search: { provider: 'local', options: { translations: {
      button: { buttonText: '搜索文档', buttonAriaLabel: '搜索文档' },
      modal: {
        noResultsText: '没有结果',
        resetButtonTitle: '清空条件',
        footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' },
      },
    } } },
    outline: { level: [2, 3], label: '本页' },
    docFooter: { prev: '上一篇', next: '下一篇' },
    lastUpdated: { text: '更新于' },
    darkModeSwitchLabel: '主题',
    returnToTopLabel: '回到顶部',
    sidebarMenuLabel: '目录',
  },
})
