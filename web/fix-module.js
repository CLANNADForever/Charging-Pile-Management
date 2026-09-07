// 构建后处理:将打包出的 JS 内联进 index.html,生成单文件产物。
// 三个关键点(都是踩过的坑):
// 1) 内联 <script> 必须放到 <body> 末尾(#app 之后),否则 mount 时 #app 未解析 → 蓝屏;
// 2) 所有 replace 的「替换内容」含 js 时,必须用「函数」而非字符串,
//    否则 js 里的 $&/$'/$` 会被 String.replace 当作特殊模式,破坏语法;
// 3) 数据/地图用 import 内联进 bundle,fetch 失败(file://)时走兜底。
import { readFileSync, writeFileSync } from 'node:fs'

const htmlPath = 'dist/index.html'
const jsPath = 'dist/assets/index.js'

let html = readFileSync(htmlPath, 'utf8')
const js = readFileSync(jsPath, 'utf8')

// 1) 移除 head 里的外部 <script src="..."></script>(空替换,无 $ 问题)
html = html.replace(/<script[^>]*src="[^"]*"[^>]*>\s*<\/script>\s*/, '')

// 2) 在 </body> 前插入内联 <script> —— 用函数替换,避免 $& 特殊模式
const inline = `<script>\n${js}\n</script>`
html = html.replace('</body>', () => inline + '\n</body>')

writeFileSync(htmlPath, html, 'utf8')
console.log('[fix-module] 已生成单文件 dist/index.html(JS 内联到 body 末尾)')
