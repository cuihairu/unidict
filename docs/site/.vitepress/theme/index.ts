import DefaultTheme from 'vitepress/theme'
import { installLangSwitch } from './lang-switch'
import './custom.css'

export default {
  extends: DefaultTheme,
  // enhanceApp 只在客户端跑，语言检测/跳转因此不会进 node 侧的渲染路径
  enhanceApp() {
    installLangSwitch()
  },
}