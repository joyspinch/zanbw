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
| 特殊单位运动状态（炮塔/建筑/飞行/隐藏…） | 逐位一致 | 7 变体 × 12 场景 |
| 单位间碰撞 + 空间索引 | 逐位一致 | 7 变体 × 17 场景 |

## 目录

`src/` 按**职责**分层，不按 C++ 的文件切分。上层只依赖下层，没有反向依赖。

| 路径 | 内容 |
|------|------|
| `src/core/` | 核心实体与全局状态：`Unit`、`Sim`、`UnitFinder`、`Kinematics`、`Scenario`、`Enums` |
| `src/math/` | 定点数（`Fp`）、方向（`Dir`）、静态表（`Tables`）、矩形（`Box`） |
| `src/regions/` | regions 层：区域划分、contours、split regions、合成地形 |
| `src/pathfind/` | 寻路：长路径 A*、短路径 contour 搜索、`path_t` 生命周期 |
| `src/collision/` | 单位间碰撞：碰撞规则、unit finder 查询、运动状态机各 `UM_*` 状态 |
| `src/vision/` | 视野：视距表生成、地形高度、迷雾揭开 |
| `src/creep/` | 菌毯：铺展、蔓延、消退，以及 `creep_life` 簿记结构 |
| `src/core/` 里的订单部分 | 订单：命令队列、订单池、派发（`Order.zan` / `OrderQueue.zan` / `ObjectContainer.zan`） |
| `src/core/` 里的单位类型部分 | 单位类型谓词：`UnitTypeIds.zan`（只看 id）/ `UnitStatus.zan`（看标志位） |
| `src/beds/` | 各层对拍入口与它们用的场景表 |
| `zan_sources.sh` | 各层的源文件清单，对拍脚本共用 |
| `tools/cppbed/` | **C++ 对照床**（不进仓库）：直接 `#include` 真实 `openbw/bwgame.h` |
| `AGENTS.md` | 本项目的移植纪律（含每一层踩过的坑） |

层间依赖：`core ← math ← regions ← pathfind ← collision`，`core ← math ← vision`，
`core ← creep`，`core ← order`。跨层调用一律走
下层的公开方法，不反向引用。

## 构建与验证

对拍脚本会编译两侧、跑同样的场景、逐字节 diff trace。**任何差异都是硬失败。**

```bash
bash check.sh           # 运动学层
bash check_regions.sh   # regions 层
bash check_astar.sh     # 长路径 A*
bash check_shortpath.sh # 短路径
bash check_path.sh      # 路径执行层
bash check_state.sh     # 运动状态机
bash check_state2.sh    # 特殊单位运动状态（炮塔/建筑/飞行/隐藏…）
bash check_collide.sh   # 单位间碰撞
bash check_vision.sh    # 视野层（视距表 + 迷雾揭开）
bash check_creep.sh     # 菌毯层（铺展 + 蔓延 + 消退）
bash check_order.sh     # 订单层（命令队列 + 派发 + 回收）
bash check_unittype.sh  # 单位类型谓词层（整张单位类型表）
```

单场景 / 单变体：

```bash
bash check.sh s6 900
bash check_state.sh 3 5
bash check_collide.sh 6 2
```

## 对照床依赖

`tools/cppbed/` 下的 C++ 床直接 `#include` 真实 OpenBW 源码（`bwgame.h`），
编译时需要 OpenBW 仓库在同级目录：

```
<somewhere>/
  openbw/          # OpenBW 源码
  zanbw/           # 本仓库
```

**对照床不在版本控制内**（见 `.gitignore`）——它们是开发工具，不是移植产物。
从干净克隆开始时，把床复制进来即可：

```bash
cp -r <openbw-bed-dir>/. tools/cppbed/
python3 tools/cppbed/gen_traced.py        # 生成带 trace 钩子的 openbw 副本
python3 tools/cppbed/gen_collide_traced.py
```

Zan 侧需要 `zanc`：

```bash
D:/project/zan-lang/build/zanc.exe src/beds/state_main.zan <...> --auto-stdlib -o build/state_zan.exe
```

## 为什么这么费劲

OpenBW 用了大量定点数（`fp8`）和 8 位截断的方向值，任何"等价"的改写都会
在某个边界上偏一个 bit。所以这里只认一种验证方式：**和原实现跑同一个场景，
逐字节比对每一个输出**。中途挖出来的缺陷全都记在 `AGENTS.md` 里，
按层编号（1–47），每条带"为什么隐蔽"。

挖出来的几类典型：

- **符号反向**（`{-a, b}` 里 `from` 本身是负的，取负后方向和字段名相反）
- **索引写死**（地图宽度、区域数当成常量）
- **失败时没清空结果容器**（搜索失败留下上一次的旧列表）
- **字面量后缀 ≠ 构造函数**（`12_fp8` 与 `fp8::integer(12)` 差一个移位因子）
- **池化对象的字段残留**（`path_t` 复用时 `last_collision_unit` 跨路径保留）
- **默认参数不是"可有可无"**（`consider_collision_with_moving_units` 只有
  一个调用点传 `true`）
- **判定顺序是行为**（`UM_TerrainSlide` 单位碰撞在前，`UM_FollowPath` 地形在前）
