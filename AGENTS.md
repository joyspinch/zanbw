# AGENTS.md — ZanBW

Zan 重写 OpenBW（星际争霸：母巢之战引擎）。目标是**比特级一致**：每一帧、每一个定点数
raw 值都必须与原 C++ 实现完全相同。

## 目录

| 路径 | 内容 |
|------|------|
| `src/` | Zan 移植代码（按 C++ 模块分文件） |
| `src/Fp.zan` | `fixed_point` 定点数模板的移植（fp8 / ufp8 / fp16 语义） |
| `src/Dir.zan` | `direction_t`（8 位截断方向值） |
| `src/Enums.zan` | 与 `bwenums.h` / `game_types.h` 对齐的枚举与标志位 |
| `src/Rng.zan` | LCG 随机数（`lcg_rand`） |
| `src/Tables.zan` | **机械导出**的静态表（tan_table / direction_table / repulse_adjust_table） |
| `src/Unit.zan` | 实体结构（unit_t / flingy_t / sprite_t 精简版） |
| `src/Kinematics.zan` | 方向数学 + 速度/转向积分 |
| `src/Sim.zan` | movement 状态机（UM_*）、repulse 场、边界钳制 |
| `src/Scenario.zan` | 场景描述与解析（两侧共用同一字符串格式） |
| `src/main.zan` | 无头仿真主循环 + trace 输出 |
| `src/Terrain.zan` | 合成地形规格（regions 层用，与 C++ 床逐条对应） |
| `src/Regions.zan` | regions 层数据结构（region / split_region / contour） |
| `src/RegionsCreate.zan` | `regions_create()` 的粗区域划分部分 |
| `src/RegionsRefresh.zan` | `refresh_regions()`：面积、tile 数、邻居、连通组 |
| `src/RegionsMain.zan` | 主流程 + 合并小区域 + reindex + split_regions |
| `src/RegionsContours.zan` | contours（精细边界，短线寻路依赖） |
| `src/RegionsDump.zan` | regions 层的 trace 输出（与 C++ 床同格式） |
| `src/regions_main.zan` | regions 层对拍入口 |
| `src/Pathfinder.zan` | 寻路入口：region 定位 + 长路径组装 |
| `src/AStar.zan` | 长路径 A*（region 级）+ 二叉堆 |
| `src/astar_main.zan` | 长路径 A* 对拍入口 |
| `src/ShortPath.zan` | 短路径核心结构（contour / neighbor / visited 列） |
| `src/ShortPathNeighbors.zan` | `expand_inner/outer`、`pf_local_edges_find`、`pf_add_neighbor`、`visit_area` |
| `src/ShortPathAddNeighbors.zan` | `add_neighbors` 四方向逐段移植（约 400 行） |
| `src/ShortPathSearch.zan` | `pf_mark_visited` / `pf_remove_visited_flags`（列式 visited 区） |
| `src/ShortPathRun.zan` | 短路径主循环 + `pf_generate_neighbors` + `pf_add_local_terrain` |
| `src/ShortPathNext.zan` | `pathfinder_find_next_short_path` + 两个 target 调整函数 |
| `src/shortpath_main.zan` | 短路径对拍入口 |
| `tools/cppbed/` | **C++ 对照床**：直接 `#include` 真实 `openbw/bwgame.h` |
| `tools/cppbed/regions_bed.cpp` | regions 层的 C++ 对照床 |
| `tools/cppbed/astar_bed.cpp` | 长路径 A* 的 C++ 对照床 |
| `tools/cppbed/astar_trace.cpp` | A* 逐步 trace 床（定位堆序问题用） |
| `tools/cppbed/shortpath_bed.cpp` | 短路径的 C++ 对照床 |
| `tools/cppbed/openbw_traced.h` | **加 trace 钩子的 openbw 副本**（由脚本从真源码生成） |
| `tools/cppbed/shortpath_trace2.cpp` | 短路径逐步 trace 床（编译 traced 副本） |
| `check.sh` | 运动学层一键逐位对拍 |
| `check_regions.sh` | regions 层一键逐位对拍 |
| `check_astar.sh` | 长路径 A* 一键逐位对拍 |
| `check_shortpath.sh` | 短路径搜索一键逐位对拍 |

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

# 单场景 / 单变体
bash check.sh s6 900
bash check_regions.sh 3
bash check_astar.sh 3
bash check_shortpath.sh 3
```

对拍脚本会编译两侧、跑同样的场景、逐字节 diff trace。**任何差异都是硬失败。**

手工命令：

```bash
# C++ 床
cd tools/cppbed && g++ -std=c++17 -I../../../openbw -O0 -g main.cpp -o bed.exe
./bed.exe s0 600 > build/trace_cpp.txt

# Zan 端
D:/project/zan-lang/build/zanc.exe src/main.zan src/Enums.zan src/Fp.zan src/Dir.zan \
    src/Rng.zan src/Tables.zan src/Unit.zan src/Kinematics.zan src/Sim.zan src/Scenario.zan \
    --auto-stdlib -o build/zanbw.exe
./build/zanbw.exe s0 600 > build/trace_zan.txt

diff build/trace_cpp.txt build/trace_zan.txt
```

重新生成静态表（改动 openbw 后）：

```bash
./tools/cppbed/bed.exe --dump-tables > src/Tables.zan
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
