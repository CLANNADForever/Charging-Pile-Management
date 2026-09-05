# 环境：内嵌地图(WebEngine) 复现所需
- 装了 qt6-webengine-dev-tools(qwebengine_convert_dict 缺失元凶)、重装 webengine 三件套、libvulkan-dev 头(解到 ~/opt/vulkan-hdr)。
- configure 需带: cmake -S . -B build -DVulkan_INCLUDE_DIR=$HOME/opt/vulkan-hdr/usr/include
- webengine 只链入 ncs_user(可执行)，不链入 ncs_user_ui → 无头测试不加载引擎(GLX/沙箱不炸)。
- NavigationPage 在 ncs_user 内装配(MainWindow::pushPage 动态页 + routeRequested 信号)。
- 地图 = Leaflet(jsdelivr)+OSM tiles(需外网)；无外网时灰屏但可点"外部地图打开"。
