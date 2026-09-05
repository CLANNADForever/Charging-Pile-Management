# 环境：内嵌地图(WebEngine) 复现所需
- 装了 qt6-webengine-dev-tools(qwebengine_convert_dict 缺失元凶)、重装 webengine 三件套、libvulkan-dev 头(解到 ~/opt/vulkan-hdr)。
- configure 需带: cmake -S . -B build -DVulkan_INCLUDE_DIR=$HOME/opt/vulkan-hdr/usr/include
- webengine 只链入 ncs_user(可执行)，不链入 ncs_user_ui → 无头测试不加载引擎(GLX/沙箱不炸)。
- NavigationPage 在 ncs_user 内装配(MainWindow::pushPage 动态页 + routeRequested 信号)。
- 地图 = Leaflet(jsdelivr)+OSM tiles(需外网)；无外网时灰屏但可点"外部地图打开"。
- 补充：Qt6 运行时还需 `libqt6webenginecore6-bin`(/usr/lib/qt6/libexec/QtWebEngineProcess)；此前缺失导致点导航即
  "Could not find QtWebEngineProcess" 崩溃。已装(用户)。
- 内嵌页加载 **腾讯地图 URI** `apis.map.qq.com/uri/v1/routeplan`(免 key)；仍留外部打开兜底。
- 若下次又遇 "Could not find QtWebEngineProcess"：`ls /usr/lib/qt6/libexec/QtWebEngineProcess`。
