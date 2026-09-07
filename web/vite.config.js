import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// base: './' + IIFE 输出,使构建产物可用 file:// 双击打开(无 module CORS 限制)。
// '/api' 代理到本仓库后端 ncs_server(默认 127.0.0.1:8080)，大屏取数走 stats/overview|daily。
export default defineConfig({
  plugins: [vue()],
  base: './',
  build: {
    assetsInlineLimit: 10 * 1024 * 1024, // 10MB:强制图片等资源内联成 base64,保证单文件可 file:// 打开
    modulePreload: false,
    rollupOptions: {
      output: {
        format: 'iife',
        inlineDynamicImports: true,
        entryFileNames: 'assets/index.js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/[name][extname]'
      }
    }
  },
  server: {
    host: true,
    port: 5173,
    proxy: {
      '/api': { target: 'http://127.0.0.1:8080', changeOrigin: true }
    }
  }
})
