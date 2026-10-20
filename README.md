# ZanBW

OpenBW（星际争霸：母巢之战引擎）的 **Zan 逐位一致重写**。

目标不是"能跑"，而是**每一帧、每一个定点数 raw 值都与原 C++ 实现完全相同**。
每一个移植层都用 C++ 原实现当参照床做差分验证，差分不一致就是硬失败。

## 当前进度

| 层 | 状态 | 对拍规模 |
|---|---|---|
| 运动学（方向数学 + 速度/转向积分） | 逐位一致 | 5 场景 × 600 帧 |
| regions（区域划分 + contours） | 逐位一致 | 7 地形变体 |
| 长路径 A*（region 级） | 逐位一致 | 7 变体 × 12 查询 |
| 短路径（contour 级） | 逐位一致 | 7 变体 × 10 查询 |
| 路径执行层（`path_t` 生命周期） | 逐位一致 | 7 变体 × 10 场景 |
| 运动状态机（`execute_movement`） | 逐位一致 | 7 变体 × 16 场景 |

## 目录

| 路径 | 内容 |
|------|------|
| `src/` | Zan 移植代码（按 C++ 模块分文件，函数名与原实现保持一致） |
| `src/Terrain.zan` | 合成地形规格（7 个变体）——**C++ 床与被测物共用这一份** |
| `src/Regions*.zan` | regions 层：区域划分、contours、split regions |
| `src/Pathfinder.zan` / `src/AStar.zan` | 寻路入口与长路径 A* |
| `src/ShortPath*.zan` | 短路径（contour 级）搜索 |
| `src/Path.zan` / `src/PathProgress.zan` | `path_t` 生命周期与 `path_progress` |
| `src/StateMachine.zan` / `src/MovementStates.zan` | 运动状态机调度器与各 `UM_*` 状态 |
| `src/Unit.zan` / `src/Sim.zan` / `src/Kinematics.zan` | 实体结构、移动积分、repulse 场 |
| `src/Fp.zan` / `src/Dir.zan` / `src/Tables.zan` | 定点数、方向、静态表 |
| `tools/cppbed/` | **C++ 对照床**：直接 `#include` 真实 `openbw/bwgame.h` |
| `AGENTS.md` | 本项目的移植纪律（含每一层踩过的坑） |

## 构建与验证

对拍脚本会编译两侧、跑同样的场景、逐字节 diff trace。**任何差异都是硬失败。**

```bash
bash check.sh           # 运动学层
bash check_regions.sh   # regions 层
bash check_astar.sh     # 长路径 A*
bash check_shortpath.sh # 短路径
bash check_path.sh      # 路径执行层
bash check_state.sh     # 运动状态机
```

单场景 / 单变体：

```bash
bash check.sh s6 900
bash check_state.sh 3 5
```

## 对照床依赖

`tools/cppbed/` 下的 C++ 床直接 `#include` 真实 OpenBW 源码（`bwgame.h`），
编译时需要 OpenBW 仓库在同级目录：

```
<somewhere>/
  openbw/          # OpenBW 源码
  zanbw/           # 本仓库
```

编译命令（床的源文件头部注释里也写了）：

```bash
cd tools/cppbed && g++ -std=c++17 -I../../../openbw -O0 -g state_bed.cpp -o state_bed.exe
```

Zan 侧需要 `zanc`：

```bash
D:/project/zan-lang/build/zanc.exe src/state_main.zan <...> --auto-stdlib -o build/state_zan.exe
```

## 为什么这么费劲

OpenBW 用了大量定点数（`fp8`）和 8 位截断的方向值，任何"等价"的改写都会
在某个边界上偏一个 bit。所以这里只认一种验证方式：**和原实现跑同一个场景，
逐字节比对每一个输出**。中途挖出来的缺陷（符号反向、索引写死、结果容器
失败时没清空……）全都记在 `AGENTS.md` 里。
