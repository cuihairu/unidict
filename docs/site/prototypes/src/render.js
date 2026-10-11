// 原型出图脚本：docs/site/prototypes/src/prototype.html → 八张 PNG
// （四页 × 亮暗双主题），落 docs/site/prototypes/assets/。
// 依赖：npx playwright（浏览器已在本机 ~/.cache/ms-playwright）。
//   node render.js            # 全量 8 张
//   node render.js 1 light    # 只出主检索页亮色（调试用）
const path = require("path");
const fs = require("fs");

let chromium;
try {
  ({ chromium } = require("playwright"));
} catch {
  ({ chromium } = require(path.join(process.env.HOME,
      ".npm/_npx", "playwright")));
}

const SRC = "file://" + path.resolve(__dirname, "prototype.html");
const OUT = path.resolve(__dirname, "..", "assets");

const PAGES = [
  ["1", "search"],
  ["2", "dictionaries"],
  ["3", "vocabulary"],
  ["4", "settings"],
];
const THEMES = ["light", "dark"];

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const onlyPage = process.argv[2];
  const onlyTheme = process.argv[3];
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1360, height: 1000 } });
  for (const [num, slug] of PAGES) {
    for (const theme of THEMES) {
      if (onlyPage && num !== onlyPage) continue;
      if (onlyTheme && theme !== onlyTheme) continue;
      // file:// 只换 hash 不会重载页面（内联 hash 脚本只跑一次）——
      // 先去空白页再进目标 hash，保证每张图都按自己的页/主题渲染
      await page.goto("about:blank");
      await page.goto(`${SRC}#p=${num}&t=${theme}`);
      await page.waitForTimeout(150); // 字体/主题类生效
      const board = page.locator(".board:visible");
      await board.screenshot({
        path: path.join(OUT, `${slug}_${theme}.png`),
      });
      console.log(`${slug}_${theme}.png`);
    }
  }
  await browser.close();
})();
