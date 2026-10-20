# AGENTS.md — ZanBW

Zan 重写 OpenBW（星际争霸：母巢之战引擎）。目标是**比特级一致**：每一帧、每一个定点数
raw 值都必须与原 C++ 实现完全相同。

## 目录

`src/` 按**职责**分层，不按 C++ 的文件切分。上层只依赖下层。

| 路径 | 内容 |
|------|------|
| `src/core/` | 核心实体与全局状态：`Unit`、`Sim`、`UnitFinder`、`Kinematics`、`Scenario`、`Enums`；`Sim` 还持有路径对象池（`FreePaths`） |
| `src/math/` | 定点数（`Fp`）、方向（`Dir`）、静态表（`Tables`）、矩形（`Box`） |
| `src/regions/` | regions 层：区域划分、contours、split regions、合成地形 |
| `src/pathfind/` | 寻路：长路径 A*、短路径 contour 搜索、`path_t` 生命周期 |
| `src/collision/` | 单位间碰撞：碰撞规则、unit finder 查询、运动状态机各 `UM_*` 状态 |
| `src/beds/` | 各层对拍入口与它们用的场景表 |
| `zan_sources.sh` | 各层的源文件清单，对拍脚本共用 |

层间依赖单向：`core ← math ← regions ← pathfind ← collision`。

### 分层的判据

代码放哪一层，看的是**它依赖谁**，不是它抄的是哪个 C++ 文件：

- 只依赖 `Unit` / `Sim` 的规则放 `core`（例：`unit_can_collide_with` 是两个单位
  之间的规则，不是"碰撞层"的规则）。
- 只用坐标和整数的几何放 `math`（例：`Box`、`is_intersecting`）。
- 只依赖 `Pathfinder` 的寻路逻辑放 `pathfind`（例：`pathfinder_unit_can_collide_with`
  是寻路的障碍过滤规则，不是碰撞规则）。
- 依赖 `StateMachine` 和 unit finder 的放 `collision`。

**反向依赖就是错的**：`pathfind` 不许引用 `collision`，`regions` 不许引用
`pathfind`。发现反向依赖时，先问"这条规则到底归谁"，再搬家，不要加
`using` 或把类型下沉硬凑。

### 场景表是一份实体，不是平行数组

对拍场景（位置、目标、延迟、路点）都是**记录**，用实体类承载，一个字段
一个含义。原版曾用 `int[] ax; int[] ay; int[] bx; ...` 按下标对齐，
改一个场景就得同时改八行 —— 这正是"多处真相"。现在：

- `src/beds/ScenarioTables.zan` 持有所有场景表。
- `PairScenario` / `StepScenario` / `UnitPlan` / `Waypoint` / `PathQuery`
  各自是一个记录。
- 路点的等待帧数挂在路点自己身上（`PendingWaypoint.Waited`），不另开一个
  按下标对齐的计数数组。

**C++ 侧的场景表是唯一真值来源**：改场景时先改 `tools/cppbed/*.cpp` 里的
C++ 表，再照抄到 Zan 的实体表，两边不能各写一份再祈祷它们一致。

### 对照床不进版本控制

`tools/cppbed/` 下的 C++ 床与生成脚本是**开发工具**，不是移植产物：它们
直接 `#include` 真实 `openbw/bwgame.h`，是逐位验证的唯一手段。仓库里不
保留它们（`.gitignore` 排除），干净克隆后从别处复制进来即可。

## 构建与验证

```bash
# 运动学层对拍（9 个场景 × 900 帧）
bash check.sh

# regions 层对拍（7 个地形变体）
bash check_regions.sh

# 长路径 A* 对拍（7 个地形变体 × 12 个查询）
bash check_astar.sh

# 短路径搜索对拍（7 个地形变体 × 10 个查询）
bash check_shortpath.sh

# 路径执行层对拍（7 个地形变体 × 10 个场景）
bash check_path.sh

# 运动状态机对拍（7 个地形变体 × 16 个场景）
bash check_state.sh

# 特殊单位运动状态对拍（7 个地形变体 × 12 个场景：炮塔/建筑/飞行/隐藏/掩体…）
bash check_state2.sh

# 单位间碰撞层对拍（7 个地形变体 × 17 场景，两单位对撞）
bash check_collide.sh

# 单场景 / 单变体
bash check.sh s6 900
bash check_regions.sh 3
bash check_astar.sh 3
bash check_shortpath.sh 3
bash check_path.sh 3
bash check_state.sh 3
bash check_state2.sh 3
bash check_collide.sh 5 3
```

对拍脚本会编译两侧、跑同样的场景、逐字节 diff trace。**任何差异都是硬失败。**

手工命令：

```bash
# C++ 床
cd tools/cppbed && g++ -std=c++17 -I../../../openbw -O0 -g main.cpp -o bed.exe
./bed.exe s0 600 > build/trace_cpp.txt

# Zan 端（源文件清单在 zan_sources.sh 里，各脚本共用）
D:/project/zan-lang/build/zanc.exe src/beds/kinematics_main.zan \
    src/math/Fp.zan src/math/Dir.zan src/math/Tables.zan src/core/Rng.zan \
    src/core/Enums.zan src/core/Unit.zan src/core/Sim.zan src/core/Kinematics.zan \
    src/core/Scenario.zan \
    --auto-stdlib -o build/zanbw.exe
./build/zanbw.exe s0 600 > build/trace_zan.txt

diff build/trace_cpp.txt build/trace_zan.txt
```

重新生成静态表（改动 openbw 后）：

```bash
./tools/cppbed/bed.exe --dump-tables > src/math/Tables.zan
```

## 翻译纪律（踩过的坑）

### 1. 定点数语义必须逐条对齐 `util.h`

`fixed_point` 是模板，实例化参数决定一切：

- `fp8 = fixed_point<24, 8, true>`：raw 存 `int_fast32_t`（本机 **4 字节**），
  所以 raw 运算是 **32 位回绕**。
- `direction_t = fixed_point<0, 8, true, true>`：raw 存 `int_fast8_t`（**1 字节**），
  `from_raw()` 每次都做 **8 位符号扩展截断**。
- `operator*(fixed_point)`：64 位中间量 `(a.raw * b.raw) >> n_fractional_bits`，
  **向负无穷取整**。
- `operator/(fixed_point)`：64 位中间量 `(a.raw << n_fractional_bits) / b.raw`，
  **向零取整**。
- `operator/(int)`：纯整数除法 `from_raw(raw / int)`，**不再移位**。
- `operator*(int)`：纯整数乘法 `from_raw(raw * int)`。
- `truncate()` / `extend()`：位移量 = `n_fractional_bits - result_fractional_bits`。
  fp8↔direction 的小数位都是 8，**位移量是 0**（曾误写成 8，浪费一轮对拍）。
- `multiply_divide` / `divide_multiply`：注意 `is_native_fast_int` 分支
  （32 位路径 vs 64 位路径），本机 `int_fast32_t` 是 4 字节 → 走 32 位路径。

### 2. 字面量后缀的含义

```cpp
12_fp8       // fp8::from_raw(12)  —— raw 就是 12，不是 12<<8！
fp8::integer(12)  // raw = 12 << 8 = 3072
```

两种写法在 openbw 里**都出现**，且值差 256 倍。翻译时逐个对照 C++ 原文，
不要凭直觉。

### 3. 无符号视图会改变除法结果

`unit_turn_rate()` 里 `uturn_rate /= 2u` 是**无符号**除法：
`5/2=2`（不是 3）。同一函数的 `update_unit_heading()` 用的是
`truncate(flingy_turn_rate)`，**不除 2**。两处不一致是原实现的事实。

### 4. `bw_insert_list` 插在索引 1，不是头插

```cpp
static void bw_insert_list(cont_T& cont, T& v) {
    if (cont.empty()) cont.push_front(v);
    else cont.insert(std::next(cont.begin()), v);   // 索引 1
}
```

这决定了 `visible_units` 的遍历顺序，进而决定**每帧单位更新顺序**和
repulse 场的相互影响。Zan 侧必须复刻同样的插入位置（`list.Insert(1, u)`）。

### 5. RNG 调用顺序即状态

`lcg_rand(source)` 会推进**同一个** LCG 状态。少一次或多一次调用，
后面所有数字全错。`create_unit` 会调 `lcg_rand(14)`，`initialize_unit` 会调
`lcg_rand(15)`，`increment_repulse_field` 会调 `lcg_rand(37)`。
trace 末尾的审计行（各 source 调用次数 + 最终状态）是对拍的一部分。

### 6. `direction_t` 的算术是 8 位回绕

`operator+` 不调用 `wrap()`（`wrap()` 只在复合赋值里调用），但 `from_raw()`
会把结果截到 8 位。所以 `Dir + Dir` 之后再 `FromRaw` 的净效果是 int8 回绕。

### 7. 浮点与整型的边界

`xy_direction(xy)` 用 `fp8::integer(pos.y) / pos.x`——**整数除法**，
不是 fp8 除法。两者结果差 256 倍。

### 8. 移植时优先选 class 而非 struct：值语义会静默丢写

C++ 里 `regions_t::region` 全程以指针传递（`region*` 放进邻居表、就地改字段）。
Zan 的 `struct` 是**值语义**——`Region r = list[i]` 拿到的是**副本**，
`++r.TileCount` 改的是副本，编译器一声不吭。移植这类"实体被到处传引用"的
C++ 结构时，一律用 `class`（引用语义）。

判据：**C++ 里如果这个类型是指针参数或存进指针容器，就用 class。**

### 9. 同一模块里有两个不同粒度的"可走"概念

regions 层里同时存在：

- **8×8 单元网格**（`unwalkable_flags`，`is_walkable(walk_x, walk_y)`）——
  由 mega tile 的 vf4 位展开而来，**region 划分、邻居判定、contour 都用它**
- **tile 级**（`is_walkable(xy)`，读 `st.tiles[].flags`）——
  用于单位能否站在某 tile 上

混淆两者会让 split_regions / 邻居判定全错。移植时逐个调用点确认用的是哪个。

### 10. tile flag 的位值不要凭记忆

`tile_t` 的标志位是：

| 标志 | 值 |
|------|-----|
| `flag_walkable` | 0x1 |
| `flag_has_creep` | 0x40 |
| `flag_very_high` | 0x100 |
| `flag_middle` | 0x200 |
| `flag_high` | 0x400 |
| `flag_occupied` | 0x800 |
| `flag_partially_walkable` | **0x2000** |

`flag_partially_walkable` 不是 0x800（那是 `flag_occupied`）。写错会让
split_regions 整条路径静默不执行——**测试要专门覆盖它**。

### 11. 合成数据也要覆盖"少见路径"

regions 层第一次只对上了粗区域，`split_regions` 恒为 0——因为合成地形里
没有 partially-walkable tile。**覆盖缺口不会自己暴露**，要主动设计地形变体
（螺旋、棋盘、窄门、嵌套房间、部分可走）去逼出不同代码路径。

### 12. `std::sort` 不稳定，但只在有重复键时才要紧

contour 按 `(v[0], v[1])` 排序。Zan 侧用稳定插入排序——只要没有两个
contour 的 `(v[0], v[1])` 相同，结果就与 `std::sort` 一致。移植前先确认
参照输出里没有重复键（`sort | uniq -d`），否则得实现同形状的非稳定排序。

### 13. 手写堆的上浮与下沉，比较条件天然相反

openbw 的 `binary_heap_up` / `binary_heap_down`：

```cpp
// up
if (compare(*parent, *element)) break;    // 父更小 → 已满足堆序，停止
// down
if (!compare(*child, *element)) break;     // 子不更小 → 已满足堆序，停止
```

上浮的条件是 `Less(parent, element)`，下沉是 `!Less(child, element)`——
**一个是"是"、一个是"非"**。写反了堆序就不对，表现为 A* 弹出错误的节点，
但路径可能仍然正确（因为代价相同时顺序不影响结果），**必须对比 nodes 数
和 highest_open 才能发现**。

### 14. 移植时区分"索引的两层"

A* 里 `open` 堆存的是 **node 指针**，而 node 有 `region` 字段。移植到
Zan 后：

- `Open` 存 **Nodes 索引**
- `Nodes[i].RegionIndex` 才是 **region 索引**
- `Region.Index` 是 region 在 `regions` 数组里的下标

三者含义不同但常常数值相等，写混了会在跨组二次搜索时静默走错。

### 15. `fixed_point * int` 不等于 `fixed_point * fixed_point`

openbw 里 `cost *= 2` 走的是 `fixed_point::operator*=(T integer_value)`，
即 **raw 直接乘**，无移位。而 `fp8 * fp8` 走 64 位中间量再右移 8 位。
两者差 256 倍。移植时逐个确认运算符重载落到哪个版本。

### 16. 跨组二次搜索时 goal_node 会被覆盖

`pathfinder_find_long_path` 在源与目标不同组时**反向搜一次**，若没直接
到达再正向搜一次。`goal_node` 是两次搜索共用的变量，第二次搜索会覆盖它。
移植时不能缓存第一次的 goal 区域。

### 17. 短路径：搜索目标不是最终目标，而是长路径的第 i+2 跳

`pathfinder_find` 走的是 `pathfinder_find_next_short_path`，它**只算一跳**：

- `target = pf.destination`，但若 `long_path.size() - current_long_path_index > 3`，
  目标改成 `long_path[index + 2]` 的**区域中心**（fp8 → 像素要右移 8）。
- `target_region = long_path[index + 1]`（`index` 是长路径最后一跳时为 null）。
- `current_long_path_index` 在 `pathfinder_find_long_path` 末尾被设成
  `(size_t)0 - 1`，进入下一跳时先 `++`。

移植时直接把 `target` 设成最终目标，短路径会一路搜到终点——路径点数、
节点数全错，而**长路径完全正确**，很容易误判成"短路径层没问题"。

### 18. 短路径：`target_is_destination` 是坐标比较，不是区域比较

```cpp
bool target_is_destination = target == pf.destination;
```

它参与 `is_exhausted` 的判定：

```cpp
if (n_open_nodes_in_target_region <= 0 || has_found_goal
    || (target_is_destination && all_nodes.size() < 150)) {
    is_exhausted = is_tired;      // 不强制耗尽
} else {
    is_exhausted = true; is_tired = true;
}
```

写成"目标区域 == 源区域"会让搜索在碰到目标区域时提前停住，
路径只剩 3 个点而不是 7 个。

### 19. 短路径：`w.inner` / `w.outer` 来自单位尺寸，不是常量

```cpp
w.inner[0] = unit_type->dimensions.from.y;   w.outer[0] = from.y + 1;
w.inner[1] = -unit_type->dimensions.to.x;    w.outer[1] = -to.x - 1;
w.inner[2] = -unit_type->dimensions.to.y;    w.outer[2] = -to.y - 1;
w.inner[3] = unit_type->dimensions.from.x;   w.outer[3] = from.x + 1;
```

它们是 contour 的膨胀量，直接决定每个邻居的位置。硬编码成 ±1 会让
路径整体偏移（实测 375 vs 366，差 outer 与 inner 的 9 像素）。

### 20. 短路径：`ShortContour` 必须是 class，但 expand_* 必须复制

C++ 的 `regions_t::contour` 是**值类型**，`expand_inner(c)` / `expand_outer(c)`
按值收参、返回**改过的副本**。Zan 侧 `ShortContour` 是 class（引用语义），
直接在 `c.V0 += ...` 上改会把 `local_edges` 里的**源 contour 永久改掉**。

症状：第一次展开后源 contour 的坐标被污染，后续所有方向的邻居都偏移。
修法：`expand_*` 里新建一个对象逐字段拷贝后加偏移。

**判据：C++ 里按值传参并就地改的 POD，改成 class 后必须显式复制。**

### 21. 短路径：visited 区是"列式"结构，插列要复制 y 区间

`pf_area_visited` 是 `a_vector<visited>`，每列存 `x` 和该列的 y 区间列表。
`pf_mark_visited` 插入新列时是 `insert(next, {area.from.x, cur->y})`——
**复制当前列的 y 区间**，不是留空。留空会让 `pf_remove_visited_flags`
的 mask 算错，搜索多探出几十个节点。

合并区间时 C++ 写的是 `t = std::prev(cur->y.erase(n))`——erase 后 `t`
停在被删元素的**前一个**位置，循环继续看同一槽位。移植成"erase 后 `t`
不动"会漏合并一段。

### 22. 短路径：找不到 goal 时的 best_node 选择按加权代价，不是总代价

```cpp
fp8 cost = i->estimated_remaining_cost;
if (i->region == destination_region || i->region == target_region) {
    cost += i->total_cost / 2;
} else {
    if (i->region->pathfinder_flag) cost = cost * 3 / 2;
    else if (i->region == source_region) cost *= 2;
    else cost *= 4;
}
if (i->visited) cost *= 32;
```

只有 `n_unvisited_nodes == 0 && n_open_nodes_in_target_region == 0` 时才
`is_stuck = true`；否则 `is_stuck` 保持 false。按总代价挑 best_node 会选错，
终点落在另一个像素上。

### 23. 短路径：`pathfinder_flag` 初始值是 1，且只清 move_to_region 的邻居

```cpp
for (auto* nr : move_to_region->walkable_neighbors) {
    if (nr == source_region) continue;
    nr->pathfinder_flag = 1;      // 开头置 1
}
...
for (auto* nr : move_to_region->walkable_neighbors) {
    if (nr == source_region) continue;
    nr->pathfinder_flag = 0;      // 结尾只清这些
}
```

`pathfinder_flag != 0` 决定节点是不是"neighbor region"节点，直接影响
`is_exhausted` 的触发时机。写成 0 会让搜索退化。

### 24. 逐步 trace 床要编译 openbw 的**副本**，不要手抄

短路径的 `add_neighbors` 有 400 多行、四个方向各不相同的边界逻辑，手抄
进 trace 床必然与真实现分叉。做法：用脚本从 `openbw/bwgame.h` 生成一份
`openbw_traced.h`（只在关键行插 `printf` 钩子），trace 床编译这份副本。

这样 trace 床的每一步都保证来自真实现，Zan 侧逐行 diff 才有意义。

## 运动状态机（execute_movement / UM_* 状态）

对应 openbw/bwgame.h 11031-11983。参照床 `tools/cppbed/state_bed.cpp`，
Zan 侧 `src/collision/MovementStates.zan` + `src/collision/StateMachine.zan`，
检查脚本 `check_state.sh`（7 变体 × 16 场景）。

普通地面单位（能走能转）只会走到 60 个状态里的 60 个里的一小部分。
剩下的 8 个（`UM_Turret` / `UM_BldgTurret` / `UM_Flyer` / `UM_Hidden` /
`UM_Bunker` / `UM_Lump` / `UM_LumpWannabe` / `UM_InitSeq`）要靠特殊的
单位类型或状态标志才进得去，专门的对照床是
`tools/cppbed/state2_bed.cpp` + `src/beds/state2_main.zan`，
脚本 `check_state2.sh`（7 变体 × 12 场景）。

## 路径执行层（path_t / path_progress / unit_path_to）

对应 openbw/bwgame.h 10751-10906。参照床 `tools/cppbed/path_bed.cpp`，
Zan 侧 `src/pathfind/Path.zan` + `src/pathfind/PathProgress.zan`，检查脚本 `check_path.sh`
（7 变体 × 10 场景）。

### 25. `pathfinder_adjust_target_pos` 的包围盒是 `-from`，不是 `+from`

```cpp
rect unit_type_inner_bounding_box(const unit_type_t* unit_type) const {
    return {-unit_type->dimensions.from, unit_type->dimensions.to};
}
```

`from` 是**负的**（如 -8），取负后变成 +8，于是目标周围的盒是
`{target - from, target + to}` = `{target + 8, target + 8}`。写成
`target + from` 会让盒偏移到反方向，落在墙里时调整结果完全不同。

**为什么隐蔽**：前几层的 10 条查询全部落在可走地面上，`adjust_target_pos`
第一次真正生效是在路径执行层——所有更早的测试都覆盖不到这个符号。

### 26. `pathfinder_find_long_path` 末尾把 `current_long_path_index` 置 -1

```cpp
pf.current_long_path_index = (size_t)0 - 1;
```

紧接着 `pathfinder_find_next_short_path` 先 `++` 再用，所以同 region 分支
实际从 0 开始。漏掉这行 ⇒ 索引停在末尾 ⇒ `++` 后越界 ⇒ 短路径为空。

### 27. `unit_type_inner_bounding_box` 与 `unit_type_bounding_box` 差 1

前者是 `{-from, to}`，后者是 `{origin - from, origin + to + xy(1,1)}`。
两个都叫 bounding box，差一个像素，用错会让贴边判定差 1 像素。

### 28. 移动目标只有一份真相：`unit.move_target.pos`

C++ 里 `u->move_target.pos` 和 `u->sprite->position` 分离；Zan 侧 `Unit`
把 `MoveTargetPos` 放在 unit 上，路径层如果自己再存一份 move target，
会和 `Sim.IsAtMoveTarget()`（比较 `Sprite.Position == MoveTargetPos`）
脱节 ⇒ 单位以为自己到了目标就停下。路径层一律读 `U.MoveTargetPos`。

### 29. 短路径的路径点裁剪：`path[49] = path.back(); path.resize(50)`

Zan 侧 `List` 没有 `resize`，且**不能**边遍历边替换自身
（`ShortPathPoints = new List<>()` 之后原列表已丢）。必须先写
`points[49]`，再新建列表装前 50 项。

### 30. 参照床必须用真 `create_unit`，不能手搓 unit_t

`unit_path_to` 读 `u->sprite->position`，手搓的 `unit_t` 没有 sprite ⇒
段错误。而且 `create_unit` 会顺带设好初始 movement flag（`0x1`）、
`move_target`、`next_movement_waypoint`——这些初始状态直接决定
第一帧的行为，必须让两侧一致。

### 31. 参照床的地形必须与 `src/regions/Terrain.zan` 逐条对齐

包括 `tile_flags` 和 `mega_tile_index` 两个函数：variant 6 是
"partially walkable"地形，flag 是 `flag_partially_walkable | flag_walkable`、
mega tile 是 2，漏了会让 region 数量完全不同——**床和被测物不一致时，
对拍失败会被误判成移植缺陷**。两边都从 `Terrain.zan` 取，别复制一份。

### 32. 状态机的两阶段地形碰撞：`terrain_no_collision_bounds` 先收缩再扫描

`check_unit_movement_terrain_collision` 不是"返回一个 bool"，它是两步：

1. **收缩**：把 `u->terrain_no_collision_bounds`（一个"已知无地形"的盒）
   按 c0..c3 四个方向的 contour 迭代缩小，写回 unit。
2. **扫描**：再按移动方向做四个方向的 `upper_bound`/`lower_bound` 扫描，
   命中即判定碰撞。

移植时**只写第一步**会得到"永远不碰撞"的错误结果。

### 33. `unit_type_inner_bounding_box` 是 `{-from, to}`，符号相反

`unit_type_inner_bounding_box(ut, origin)` 返回 `{origin - from, origin + to}`，
而 `from` 本身是负的 ⇒ 盒是 `origin + |from|`。写 `origin + from` 会把盒
缩小方向搞反，`is_blocked` 判错。

### 34. 短路径搜索会**清空** `pf.short_path`，失败时留下空列表

C++ `pathfinder_find_short_path` 在重建前无条件 `pf.short_path.clear()`。
Zan 侧若只在成功时才 `Pf.ShortPath = points`，失败时会**留下上一次的
旧列表**⇒ `path_progress` 误判"有路径"⇒ 状态机不进入 `FailedPath`，
而是继续跟一条已经不存在的路径。赋值必须无条件做。

### 35. `tile_t::flag_has_creep` 是 `0x40`，不是 `0x100`

`0x100` 是 `flag_very_high`。`is_walkable` 判错 ⇒ 地面单位在有残骸的
格子上被判为非法位置 ⇒ 反复进 `CheckIllegal`。

### 36. `Sim.TileIndex` 的地图宽度不能写死

openbw 是 `uy * game_st.map_tile_width + ux`，写死 128 在 64 宽的床上
索引全错 ⇒ 读到别的 tile 的 flag。tile 尺寸要从地图尺寸算。

### 37. 状态机层的边界：单位间碰撞不在这一层

`check_unit_movement_unit_collision` / `check_unit_movement_terrain_collision`
里的单位间部分需要 unit finder（所有单位的空间索引），本层没有。
单单位床里恒为"无碰撞"。**但 `is_blocked` 里的 `unit_type_can_fit_at`
（地形判定）必须移植**——它决定单位会不会反复进 `CheckIllegal`。

### 38. `UM_CheckIllegal` 的"找合法位置"分支要移植

单位卡在墙里时，C++ 会**搜索**一个合法位置并用
`create_single_step_path` 走过去，而不是原地卡死。跳过这段 ⇒
单位永远停在非法位置，两侧状态机立刻分叉。

## 碰撞层（unit finder / UM_FixCollision / UM_WaitFree / 路径续算）

对应 openbw/bwgame.h 11465-11983。参照床 `tools/cppbed/collide_bed.cpp`，
Zan 侧 `src/collision/`，检查脚本 `check_collide.sh`（7 变体 × 17 场景）。

### 39. 路径对象是**池化**的，字段会跨路径残留

```cpp
path_t* new_path() {
    // We need a (first-in-last-out) free list of paths since
    // last_collision_unit is reused.
    if (!st.free_paths.empty()) { ...pop_front... }
    ...
}
void free_path(path_t* path) { st.free_paths.push_front(*path); }
```

`new_path()` 复用对象时**只重置一部分字段**：`delay` / `creation_frame` /
`state_flags` / `long_path` / `short_path` / `current_*_index` / `source` /
`destination` / `next` 被写了，而 `last_collision_unit`、
`last_collision_speed`、`slide_free_direction` **保留上一次的值**。

Zan 侧若每次 `new Path()`，`last_collision_unit` 恒为空 ⇒
`UM_TurnAndStart` / `UM_Repath` 取不到"上次撞到的单位" ⇒ 新路径不绕开它。
必须照抄 free list（先入后出）。

**为什么隐蔽**：这个字段只在"单位撞到别人后重规划"时才有值，路径层
（单单位床）永远测不到；只有两单位对撞的床才会暴露。

### 40. `consider_collision_with_moving_units` 是路径层的第四个参数

```cpp
bool unit_path_to(unit_t* u, xy to,
    const unit_t* consider_collision_with_unit = nullptr,
    bool consider_collision_with_moving_units = false)
```

`pathfinder_unit_can_collide_with` 里：

```cpp
if (target != consider_collision_with_unit && !consider_collision_with_moving_units) {
    if (!unit_is_at_move_target(target)) return false;   // 移动中的单位不算障碍
}
```

只有 `UM_RepathMovers` 传 `true`（"给挡路的 movers 重新规划"），
其余状态（`UM_FollowPath` / `UM_StartPath` / `UM_Repath` / `UM_TurnAndStart`）
都传默认 `false`。**漏传 ⇒ 路径不绕开正在移动的单位 ⇒ 两侧路径分叉。**

### 41. `check_unit_collision` 不能省

`UM_MoveToLegal` 里：

```cpp
if (move()) {
    ...
    check_unit_collision(u);                    // ← 少这一行就分叉
    u_set_status_flag(u, unit_t::status_flag_collision);
    u->movement_state = movement_states::UM_CheckIllegal;
}
```

`check_unit_collision` 把**互相挡路的单位**都打上 `status_flag_collision`
（不只被打的那个）。它影响下一个 frame 的
`unit_update_path_movement_state`（`u_collision(u) && u_ground_unit(u)`
⇒ 直接转 `UM_CheckIllegal`），少一行 ⇒ 一个 frame 后状态机分道扬镳。

### 42. 碰撞判定的**顺序**是行为的一部分

同一个"撞上了"判定里，单位碰撞和地形碰撞的先后会改变结果：

| 位置 | C++ 顺序 |
|---|---|
| `UM_FollowPath` | 先地形（`check_unit_movement_terrain_collision`），再单位（`check_ground_unit_movement_unit_collision`） |
| `UM_FixTerrain` | 先地形，后单位 |
| `UM_TerrainSlide` | **先单位，后地形** |

`UM_TerrainSlide` 是唯一一个单位碰撞在前的地方。顺序写反 ⇒ 该滑开的
时候没滑开，或者不该撞的时候撞了。

### 43. `create_unit` 会消耗 **两次** RNG

```cpp
u->wireframe_randomizer = lcg_rand(15) & 0xff;   // initialize_unit 里
```
加上 `create_unit` 开头的 `lcg_rand(14)`，一个单位就推进两次 RNG 状态。
床里手搓单位时漏掉 ⇒ 后续所有 `lcg_rand(50, ...)` 的取值全部错位 ⇒
"找合法位置"的方向和长度不同 ⇒ 两侧停在不同的位置。

### 44. `path_t` 的 `long_path` 是 `circular_vector`，`resize` 保留首元素

`create_single_step_path` 里 `path->long_path.resize(1)`：vector 已有元素时
`resize` 只销毁尾部，**保留 `long_path[0]`**；空 vector 才补一个 nullptr。
Zan 侧 `List` 没有这个语义，要显式写"只留第一个"。

### 45. 逐位对拍的定位顺序：先比**状态链**，再比**路径**，最后比**数值**

`execute_movement` 一帧内会跑完多个状态（`FixCollision → WaitFree → ...`），
只看帧末的状态会漏掉分叉点。打印**每帧的状态序列**（C++ 侧在
`switch` 前抓一次状态、break 前记一次），第一个不同的帧就是分叉点。

分叉点的三类典型症状与下一步：

| 症状 | 去查 |
|---|---|
| 某状态多/少了一次 | 该状态内部的判定顺序、缺移植的分支 |
| 状态相同但 `waypoint`/`path` 不同 | 路径生成（对象池残留、moving-units 标志、长路径索引） |
| 状态相同、路径相同，但坐标/速度不同 | RNG 消耗数、fp8 字面量语义 |

### 46. `Fp.RawLiteral`：C++ 的 `_fp8` 字面量是**原始值**

`1_fp8` = raw 1（= 1/256 像素），`256_fp8` = raw 256（= 1.0），
`192_fp8` = raw 192。C++ 里 `fp8::integer(x)` 才是"x 个整像素"（raw = x*256）。
两者差 256 倍。C++ 源里 `fp8::integer(...)` 和 `_fp8` 字面量混用，
照抄时必须逐个核对，不能统一按"像素"理解。

Zan 侧用 `Fp.RawLiteral(n)`（raw n）和 `Fp.Integer(n)`（raw n*256）区分。

### 47. `out` 参数：调用点必须显式写 `out`，且不做重载

```java
Collision.IsBlockedWithUnit(this, u, posX, posY, out blockingUnit);   // 对
Collision.IsBlockedWithUnit(this, u, posX, posY, blockingUnit);        // 段错误
```

漏写 `out` 时参数按值传，`out` 的写回落到临时对象上 → 崩在调用表达式处
（函数体一行都没执行）。

另外**不要用 `out` 做方法重载**，Zan 的重载解析在 `out` 参数上不可靠：
带 `out` 的版本起一个**独立名字**（`IsBlocked` / `IsBlockedWithUnit`），
不要写成 `IsBlocked` 的两个重载。

### 48. 对照床的地形数据必须共用一份，不能各写各的

新开一层对拍床时，最容易犯的错是"顺手再写一份地形"：床 A 用
`mega_tile_index = 墙 ? 1 : 0`，床 B 用 `墙且邻接 ? 1 : 孤立墙 ? 2 : 0`。
两边**看起来**都是"墙=不可走"，但 mega tile 的编号含义不同 →
`game_st.vf4` 的查表结果不同 → 区域分解不同 → 路径从根上就分叉，
后面每一帧的数值都对不上。

**规则**：所有对拍床的地形函数都从 `tools/cppbed/regions_bed.cpp` 抄，
和 `src/regions/Terrain.zan` 一一对应。地形函数一改，两边一起改。
症状是"区域编号不一样"（比如 C++ 报 region 11、Zan 报 region 63）。

### 49. 精灵的高度层级来自**单位类型**，不是 `create_sprite` 的默认值

`create_sprite()` 把 `sprite->elevation_level` 设成 4，
`initialize_unit_type()` 紧接着用 `unit_type->elevation_level` 覆盖它。
`UM_Init` 用 `sprite->elevation_level < 12` 决定走地面状态还是
`UM_Flyer`，所以**飞行单位必须让精灵高度 = 单位类型高度**（12）。
床里手搓单位时漏了这一步，飞行单位会走进 `UM_AtRest` 而不是 `UM_Flyer`。

### 50. 落地建筑的初始化有副作用：进 unit finder + 一次碰撞检查

`initialize_unit()` 对 `u_grounded_building(u)` 的单位做三件事：
`unit_finder_insert(u)`、`set_unit_tiles_occupied(...)`、
`check_unit_collision(u)`。最后一项会给**互相重叠的单位**打
`status_flag_collision`（0x40000）——差这一位，状态机的走向就变了。

注意 `unit_finder_insert(u)` 对**炮塔**直接 return（炮塔不进空间索引），
但 `check_unit_collision(u)` 照做。床里手搓建筑时这两件事要分开处理。

### 51. `immovable` 标志会被两处先后改写

`set_unit_move_target()` 在改移动目标时**清掉** `status_flag_immovable`；
路径层 `path_progress()` 算完后又按 `pf.is_stuck` **重新设上**（而且会把
移动目标改成 `pf.destination`）。所以"单位不动"这个结果不是一条路径
决定的，而是这两处的叠加。移植时漏掉任何一处，状态标志和移动目标都会差。

## 已知 zanc 缺陷

- **一元运算符重载**（`operator-`）在非整型上会触发 irgen bug
  （`LLVM verification failed: %neg = sub ptr null`）。
  移植时一律改用方法（`Neg()` / `Abs()`）。这是编译器缺陷，不是移植绕路。
- 裸数组初始化器 `{...}` 不可解析，必须 `new long[] {...}`。
- enum 成员是 32 位有符号，`0x80000000` 放不下。
- 顶层 `const` 不支持，要放进类里。
- 类的 `const` 字段不能作 switch 标签（必须是编译期常量），所以标志位用
  原生 `enum` 表达。

## 扩展方法

新增一个移植模块时：

1. 在 `src/` 加对应文件，函数名和 C++ 原函数名保持一致（便于对照）。
2. 在 C++ 侧把该模块的行为加进 trace 的输出字段。
3. 在 `src/Scenario.zan` 和 `tools/cppbed/main.cpp` 里**同步**加场景。
4. 跑 `bash check.sh` 直到全绿。

不要在 Zan 侧"顺手优化"——任何偏离 C++ 的写法都会在下一轮对拍里暴露。
