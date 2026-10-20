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
| `tools/cppbed/` | **C++ 对照床**：直接 `#include` 真实 `openbw/bwgame.h` |
| `check.sh` | 一键逐位对拍 |

## 构建与验证

```bash
# 全部场景对拍（9 个场景 × 900 帧）
bash check.sh

# 单场景
bash check.sh s6 900
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
