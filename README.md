# MiniBar

[![build](https://github.com/Hanshi8/minibar-lyrics/actions/workflows/build.yml/badge.svg)](https://github.com/Hanshi8/minibar-lyrics/actions/workflows/build.yml)

基于原版 MiniBar（[AceisX/minibar-now-playing-overlay](https://github.com/AceisX/minibar-now-playing-overlay)）的改版，新增了同步歌词功能。原版英文 README 见上游仓库。

**Windows 11 上最轻的音乐悬浮条。** 内存占用约 7 MB，可执行文件约 350 KB，重绘之间不占用 GPU，没有任何钩子或注入的 DLL，所以和反作弊软件一起运行也很安全。

<img src="https://raw.githubusercontent.com/AceisX/minibar-now-playing-overlay/main/docs/media/minibar.gif" width="480" alt="MiniBar：悬停、滚动歌名、切歌、音量">

MiniBar 读取 Windows 本来就知道的媒体会话，所以不管你用什么放音乐都能用 —— Spotify、Tidal、浏览器里的 YouTube Music、foobar2000、Windows 媒体播放器。不用账号、不用 API key、不用插件、不用登录。

## 歌词（本改版新增）

换歌时自动去网上找同步歌词，先找 LRCLIB，找不到再试网易云公开接口，不用登录、不用 API key。歌词按播放进度自动切行，长行会自动滚动。

- **任务栏常驻显示**：有歌词的歌，任务栏小药丸会自动展开，常驻显示歌名 + 当前歌词行，不用再悬停才看得到。没歌词时（比如看视频、直播）保持原样：平时只显示封面和按钮，悬停才展开看标题。
- **第二行逻辑**：有歌词显示当前歌词行，没歌词显示歌手，和以前一样。
- **视频、直播**：封面就是系统给的视频缩略图，照常显示；没有歌词就不会出现歌词行。
- **开关**：右键托盘图标 → `显示同步歌词` 可以开关歌词，也可以在 `minibar.ini` 里设置。
- **中文歌**：中文歌名、歌词正常显示（DirectWrite 会自动用系统里的中文字体补齐）。歌太冷门、LRCLIB 和网易云都没收录同步歌词时（纯音乐、直播、视频一般都没有），就只显示歌名和封面，不会报错。
- **内存**：歌词模块常驻多不到 1 MB，整机还是约 7 MB 的水平。

```ini
[lyrics]
enabled=1          ; 1=显示歌词，0=关闭（等同于菜单里的开关）
taskbar_expand=1   ; 1=有歌词时任务栏药丸自动展开，0=只在悬停时展开
```

## 为什么它这么小

多数“正在播放”小部件都是套了壳的网页应用：一个 Electron 外壳，或者一个 .NET 运行时，或者一个 WebView。MiniBar 是一个 Win32 窗口，用 Direct2D 以软件方式光栅化，再用 `UpdateLayeredWindow` 呈现。没有渲染循环：有变化时才重绘，其余时间都在消息循环里睡觉。

| | 实测 |
|---|---|
| 内存（私有工作集） | 横条外观约 **7 MB**，方块约 5.5 MB |
| 可执行文件 | 约 **350 KB**（CI 编译的 release 文件），不用安装运行时 |
| 静置时 CPU | 一个核心的 **0.03%**，任务管理器里显示 **0.0%** |
| 重绘之间的 GPU | **0%**，根本没有 GPU 设备 |
| 启动到显示悬浮条 | 约 **57 ms** |

切歌的开销比什么都不做要大 —— 解码封面、取它的主色、淡入 —— 所以包含一次切歌的那一分钟，CPU 会测到百分之零点几，而不是百分之零点零几。两次事件之间进程什么都不做。刚下载的可执行文件第一次运行也会比 57 ms 慢得多，而且没法预测：Windows 会先扫描新文件才放行。

它也从不碰其他进程。没有 DLL 注入，没有 DirectX 或 Vulkan 钩子，没有全局 `SetWindowsHookEx`。它对其他应用的了解全部来自公开的、进程外的 API —— 和 Windows 音量浮窗用的同一批。这是刻意的设计约束，因为那些正是反作弊系统会盯上的模式。

**这里为什么没有对比表。** 最直接的问题是它和替代品比怎么样，老实说，我们没法在自己愿意公开的条件下测出它们。最接近的项目分发方式都不适合做便携、无安装的测试：[ModernFlyouts](https://github.com/ModernFlyouts-Community/ModernFlyouts)（WinUI/.NET）只发 MSIX 包，而且上一个版本还是 2021 年 4 月的 0.9.3；[lofi](https://github.com/dvx/lofi)（Electron）发的是 Windows 安装程序，会往注册表写卸载项，而且要先登录 Spotify 才能显示东西；[Rainmeter](https://github.com/rainmeter/rainmeter)是个皮肤引擎而不是正在播放应用，它自带的皮肤里根本没有正在播放皮肤，所以测出来的其实是别人的第三方皮肤。与其发布我们自己都没测过的数字，不如只发布我们自己的 —— 以及复现的方法：在 Windows 11 25H2 上，放着音乐、鼠标离开悬浮条，60 秒采样私有工作集和 CPU。

## 安装

1. 从[本仓库的 release](https://github.com/Hanshi8/minibar-lyrics/releases/tag/v1.2.0-lyrics)下载 `minibar.exe`。
2. 放到你喜欢的地方，运行它。

整个安装过程就是这样。可执行文件是自包含的 —— 静态 CRT，不用 VC++ 运行库，旁边也不用放 DLL —— 而且是便携的：它唯一会写的文件就是旁边的 `minibar.ini`。什么都不写进 AppData 或注册表。

Scoop 和 winget 装的是原版（没有歌词功能）：

```powershell
scoop bucket add aceisx https://github.com/AceisX/scoop-bucket
scoop install minibar
```

`winget install minibar` 正在路上：清单已经提交到社区仓库，合并后就会出现。

**系统要求：** Windows 11 x64。没有别的。

## 三种模式

|  | 悬浮 | 停靠 | 嵌到任务栏 |
|---|---|---|---|
| 位置 | 任意位置，可拖动 | 沿屏幕边缘的一条 | 在任务栏的空白处 |
| 占用屏幕空间 | 不占用 | 占用，最大化的窗口会让着它 | 不占用 |
| 静置时点击穿透 | 是 | 否 | 否 |
| 静置时不透明度 | 降低（`idle_opacity`） | 完整 | 完整 |
| 控制按钮 | 悬停时出现 | 一直显示 | 一直显示 |
| 全屏窗口化游戏上 | 看得到 | 看不到 | 看得到（任务栏可见时） |
| 可用外观 | 横条或方块 | 横条 | 紧凑药丸 |
| 默认尺寸 | 293×50 或 92×92 DIP | 高 50 DIP，侧边宽 56 DIP | 任务栏高度 |

**悬浮**，横条外观 —— 拖到任意位置，静置时点击穿透、变暗，直到你悬停上去。每次它出现 —— 启动时、从托盘或热键把它叫回来时、或者切到悬浮模式时 —— 它会先以完整不透明度显示几秒再变暗，这样即使静置透明度调得很低，你也能找到它：

<img src="https://raw.githubusercontent.com/AceisX/minibar-now-playing-overlay/main/docs/media/overlay-bar.png" width="318" alt="悬浮，横条外观">

**悬浮**，方块外观 —— 只有封面和一条进度线，悬停时控制按钮浮在上面：

<img src="https://raw.githubusercontent.com/AceisX/minibar-now-playing-overlay/main/docs/media/overlay-tile.png" width="144" alt="方块，静置"> <img src="https://raw.githubusercontent.com/AceisX/minibar-now-playing-overlay/main/docs/media/overlay-tile-hover.png" width="144" alt="方块，悬停显示控制按钮">

**停靠** —— 沿屏幕边缘的一条，会预留空间：最大化的窗口会在它上面停住，而不是盖住它。四条边都可以，从菜单里的**边缘**选。

<img src="https://raw.githubusercontent.com/AceisX/minibar-now-playing-overlay/main/docs/media/docked.png" width="952" alt="停靠到屏幕边缘">

在左右两边，这条带子又窄又高，所以用竖排布局：封面、进度条和三个控制按钮竖着排、上下居中。那里没有文字 —— 56 DIP 放不下歌名，旋转显示还不如不显示。

**嵌到任务栏** —— 任务栏空白处的一颗药丸。它跟随任务栏的高度、边缘、DPI 和主题，悬停时变宽显示歌名和歌手，任务栏自动隐藏或全屏应用接管时，它也跟着消失。

<img src="https://raw.githubusercontent.com/AceisX/minibar-now-playing-overlay/main/docs/media/taskbar.png" width="367" alt="嵌在任务栏里">

从右键菜单或托盘图标切换模式和外观。

## 使用方法

| 做什么 | 怎么做 |
|---|---|
| 显示控制按钮 | 悬停到悬浮条上 |
| 播放/暂停、上一首、下一首 | 点按钮 |
| 快进 | 点进度条（如果播放器允许） |
| 移动悬浮条 | 拖动（仅悬浮模式），位置会自动保存 |
| **播放器音量加/减** | **在悬浮条上滚动鼠标滚轮** |
| **播放器静音** | **中键点击悬浮条** |
| 调整大小 | 悬浮条上按住 **Ctrl** 滚动滚轮（仅悬浮模式），或改 INI 里的 `bar_width` / `tile_size` |
| 切换模式或外观 | 右键 → 模式 / 外观 |
| 把停靠的条子移到另一条边 | 右键 → 边缘（底部 / 顶部 / 左侧 / 右侧） |
| 选择跟随哪个播放器 | 右键 → 播放器；离开的那个会暂停，选中的那个会开始 |
| 浅色或深色 | 右键 → 主题（跟随 Windows / 深色 / 浅色） |
| 静置时更暗或更亮 | 右键 → 静置透明度（仅悬浮模式） |
| 开机启动 | 右键 → 开机启动 |
| 菜单 | 右键点悬浮条，或点托盘图标 |
| 显示/隐藏 | 双击托盘图标，或用热键（如果开了） |

菜单里的每一项在 `minibar.ini` 里也都有一行，文件里放的是细节 —— 精确尺寸、热键、停靠边缘。菜单只放你平时真正会改的东西。

滚轮（调音量和调大小）依赖 Windows 的设置“悬停时滚动非活动窗口”，默认就是开着的。

### 开机启动，不动注册表

**开机启动**会在你的启动文件夹 —— 用 `shell:startup` 能打开的那个 —— 里放一个快捷方式，指向 `minibar.exe` 现在的位置。什么都不写进注册表，什么都不安装，你也可以手动删掉那个快捷方式，MiniBar 不会因此出错：菜单里复选框的状态是看文件在不在，而不是看自己的设置。

### 音量是播放器的，不是系统的

MiniBar 动的是播放器自己在 Windows 混音器里的滑块，所以你机器上别的东西音量都不会变。是哪个进程，由媒体会话自己说了算：它的 `SourceAppUserModelId` 会和每个混音器会话的拥有者比对，按可执行文件名比对，商店应用按包身份比对。

如果播放器没有音频会话 —— 没在运行，或者从来没出过声 —— 就没什么可调的，滚轮会变回调悬浮条大小，不用按 Ctrl。音量会在进度条自己身上显示一秒半，用浅色而不是专辑色：不弹出面板，不多占一个像素。

Core Audio 接口是第一次用滚轮时才创建的，不是启动时。如果你从来不动音量，就永远不用付这笔开销。

### 同时有两个播放器

任何注册了媒体会话的东西都会出现在菜单的**播放器**下面 —— 包括浏览器标签页里的一个视频。选一个，MiniBar 就跟随它；选**自动**，就跟随 Windows 认为当前的那一个。

浏览器会显示真名。Firefox 和网页应用注册媒体会话时用的是一个看不懂的标识符 —— Firefox 用的是它安装路径的十六进制哈希 —— 看起来不像任何进程名，菜单里会显示成 `F9EA399AADDF5B80` 这种东西。MiniBar 会去问桌面的窗口它们声明了哪个标识符，找到匹配的那一个，再用可执行文件自己的名字。这也是滚轮调音量和最小化对浏览器生效的原因：没有它，就没法知道那个标识符属于哪个进程。

切换时，你离开的播放器会被暂停，你选中的那个会开始，所以切换播放器的意思就是你想的那个意思：音乐跟着走。比较的是切换前后跟随的播放器，而不是菜单项：如果选中的就是同一个播放器，谁都不会被留下，什么都不会动。

有两个细节让这件事不烦人。暂停会作用于你离开的那个应用的所有会话 —— 一个浏览器可以有好几个 —— 而开始只作用于 MiniBar 现在跟随的那一个会话，因为一次启动三个暂停的标签页比要解决的问题还糟。如果新的播放器说它现在不能开始，MiniBar 也不会坚持。这两半可以分别关掉，从菜单或用 `pause_on_switch` 和 `play_on_switch`。

### 它会把播放器收起来

MiniBar 运行时，播放器的窗口会被最小化（可以设置），这样它就不挡事 —— 而一个不再被合成的窗口也会停止重绘。在 Spotify 上实测（Windows 11 25H2，放着音乐，60 秒窗口轮换三轮）：窗口在屏幕上时约占一个核心的 10%，最小化后约 2%。省的是 CPU，不是内存 —— 内存只会从五百多 MB 里掉几十 MB —— 而且只在窗口本来会在屏幕上时才省：一个已经被别的窗口盖住的窗口，最小化后开销差不多。改成隐藏而不是最小化（`hide_instead=1`）省得并不更多，只是会同时把它从任务栏和 Alt-Tab 里拿掉。MiniBar 自己的开销见上面的表。

退出 MiniBar 时窗口**一定**会被恢复。如果上一次运行死得很难看、窗口还藏着，下次启动时会恢复它：留在磁盘上的标记也记录了那是哪个播放器。

## 颜色来自专辑封面

进度条 —— 以及背景里透出的一点颜色 —— 取封面的主色。WIC 在画封面的同时把一张 12×12 的缩略图缩放到位，色相带里权重最大的获胜：每个像素都按它的饱和度计分，灰色和极端值不计分，相邻的带会加在一起，这样横跨两个带的颜色就不会丢掉一半权重。结果会被推到一个在任何底色上都清晰的饱和度和亮度区间。

没有真实颜色的封面 —— 黑白的、杂乱的拼贴 —— 什么都不染，默认的绿色保持不变。整件事每首歌只花 144 个像素。

## 配置

第一次运行时 `minibar.exe` 会在自己旁边写出 `minibar.ini`。

```ini
[general]
mode=overlay          ; overlay | docked | taskbar（悬浮 | 停靠 | 嵌到任务栏）
theme=auto            ; auto | dark | light（自动 | 深色 | 浅色），自动跟随 Windows 应用主题，
                      ; 实时生效，不用重启；在游戏上面可能还是想要深色
preferred_app=Spotify ; 媒体会话 SourceAppUserModelId 的子串；
                      ; 留空 = 跟随 Windows 认为当前的那个。
                      ; 也可以从菜单设置：右键 → 播放器

[overlay]
layout=bar            ; bar（横条）| tile（方块，只有封面）
bar_width=293         ; DIP，185-840
tile_size=92          ; DIP，68-224
x=…  y=…              ; 拖动悬浮条放下后写入
idle_opacity=140      ; 0-255，静置时的不透明度

[docked]
edge=bottom           ; bottom | top | left | right（底部 | 顶部 | 左侧 | 右侧）

[taskbar]
anchor=auto           ; auto | left | center | right（自动 | 左 | 中 | 右）
                      ; 自动：应用图标居中时靠左，
                      ;       图标左对齐时居中

[player]              ; 1.0 之前叫 [spotify]，旧文件照样能读
auto_minimize=1       ; MiniBar 启动时最小化播放器
hide_instead=0        ; 1 = 用 SW_HIDE 代替 SW_MINIMIZE（更彻底）
launch_if_missing=0   ; 播放器没运行时启动它
launch_path=          ; 要启动的可执行文件，留空 = 按播放器名字猜
pause_on_switch=1     ; 从菜单切换时，暂停离开的那个播放器
play_on_switch=1      ; 并启动选中的那个，如果它之前没在播放

[volume]
wheel_step=4          ; 每滚一格音量变化的百分点，1-25

[hotkey]
enabled=0
modifiers=alt+shift
key=M

[lyrics]              ; 本改版新增
enabled=1           ; 1=显示同步歌词，0=关闭（等同于菜单里的开关）
taskbar_expand=1      ; 1=有歌词时任务栏药丸自动展开，0=只在悬停时展开
```

## 实现说明

完整设计文档 —— 想法、硬约束、性能预算和每个选择背后的理由 —— 在 [docs/design.md](docs/design.md)（意大利语）。

### 用软件渲染，不用 DirectComposition

把一个半透明置顶窗口画出来的直接办法，是 DXGI 合成交换链加 DirectComposition。这个先搭出来了，然后和更简单的备选方案对比着测了一遍 —— Direct2D 以软件方式光栅化到 WIC 位图，再用 `UpdateLayeredWindow` 呈现：

| | D3D11 + DXGI + DirectComposition | D2D 软件 + UpdateLayeredWindow |
|---|---|---|
| 私有内存 | 37.7 MB | **8.7 MB** |
| 私有工作集 | 约 34 MB | **6.1 MB** |
| 线程 | 26–29 | **10–12** |
| 句柄 | 约 1120 | 约 **320** |
| 静置时 CPU | 一个核心的 0.29% | 一个核心的 **0.10%** |

一个 D3D11 设备和它的驱动，分摊不到一个最多一秒重绘一次的 266×45 表面上。那个表面的软件光栅化只要零点几毫秒，而且永远不碰 GPU，这让“重绘之间 GPU 占用为 0”是结构性的，而不是碰运气。

### 长歌名会滚动，但只在鼠标下面时

放不下的歌名会像别处一样截断加省略号。悬停上去它就开始滚动 —— 滚出去、停一下、滚回来 —— 边缘是淡出的，而不是被切断。淡出只出现在文字确实还在继续的那一边：滚到头之后，右边缘又是清晰的。

负责动画的计时器在鼠标进入时创建、离开时销毁，歌名放得下时根本不创建。滚动时一次重绘 0.64 ms，静止时 0.39 ms —— 悬停时不到一个核心的 2%，不悬停时什么都没有。

### 过场动画需要多久就跑多久，然后计时器就没了

切歌时新封面在 220 ms 里在旧封面上淡入，进度条的颜色也跟着一起走 —— 颜色在画面还没溶完时就跳变，会比溶解本身更显眼。在任务栏里，药丸是缓缓张开而不是一下子跳开。

每个过场 —— 悬停透明度、封面、药丸 —— 都跑在一个 16 ms 的计时器上，有东西开始动时启动，全部静止的瞬间就停。加上它们前后静置时的测量结果是一样的，这就是意义所在。

### 浅色或深色：两个设置，不是一个

Windows 存了两个独立的值，MiniBar 把它们用在两个不同的地方。`SystemUsesLightTheme` 是外壳的颜色，也就是任务栏的：它决定坐在任务栏上的药丸的对比度。`AppsUseLightTheme` 是应用画自己时该跟随的，决定 MiniBar 在悬浮和停靠模式下画的背景。在 Windows 11 上你可以让任务栏深色、应用浅色，这时两者就不该一样。

<img src="https://raw.githubusercontent.com/AceisX/minibar-now-playing-overlay/main/docs/media/overlay-bar-light.png" width="318" alt="跟随浅色应用主题的悬浮条"> <img src="https://raw.githubusercontent.com/AceisX/minibar-now-playing-overlay/main/docs/media/taskbar-light.png" width="367" alt="浅色任务栏上的药丸">

这个变化是在 `WM_SETTINGCHANGE` 上实时应用的：两个值都会重读、悬浮条重绘，不用重建任何东西。INI 里的 `theme=dark` 或 `theme=light` 会覆盖它。

### 任务栏模式不是 deskband

像 deskband（`IDeskBand`）那样*在*任务栏里面画，需要 Explorer 把一个 DLL 加载进自己的进程。Windows 11 已经移除了这个机制 —— 任务栏是 XAML，不托管第三方扩展 —— 而且那本来就是注入。

MiniBar 做的，是把自己的置顶窗口放在任务栏的空闲像素*上面*。视觉上没有区别；技术上它还是一个独立的窗口，所以不会跟着应用图标一起滚动。找空闲位置时，它读 `TaskbarAl`（图标对齐方式）和 `TrayNotifyWnd`（通知区域开始的地方）的矩形，都是只读的、进程外的。它用 `ABM_NEW` 注册为 appbar，但**从不**调用 `ABM_SETPOS`，所以能收到 `ABN_POSCHANGED`、`ABN_STATECHANGE` 和 `ABN_FULLSCREENAPP`，而不占走屏幕空间。

Windows 11 的任务栏应用图标不是窗口，没法枚举：位置来自系统声明的对齐方式，而不是量按钮在哪里结束。窗口开得非常多、又是左对齐时，居中的药丸可能会压到它们上面；这时用 `anchor=right`。

### 它不会做的事

在*独占*全屏游戏上面，悬浮条是看不到的。这是设计如此，这里没有变通办法 —— 那是 Windows 游戏栏的活。现在多数游戏用的无边框窗口化是可以的。

## 编译

什么都不用安装：工具链在 `tools\` 里，是便携的，和应用本身一样。

```powershell
.\build.ps1                 # Release（默认）
.\build.ps1 -Config debug
.\build.ps1 -Clean          # 从头开始
```

可执行文件在 `dist\minibar.exe`。要在另一台机器上重建工具链本身，见 [README.it.md](README.it.md)。

## 许可证

MIT — © 2026 MiniBar contributors。见 [LICENSE](LICENSE)。
