#!/usr/bin/env python3
"""Generate openbw_traced.h: a copy of the real openbw/bwgame.h with trace
hooks in the short path search.

The short path search (pathfinder_find_short_path) is ~1100 lines with four
directions of asymmetric boundary logic. Hand-copying it into a trace bed
guarantees a divergence from the real implementation, which makes the
step-by-step comparison worthless. Instead we patch the real source and
compile the copy, so every traced step provably comes from the original.

Usage: python3 tools/cppbed/gen_traced.py [path-to-openbw]
"""

import os
import sys

NL = chr(92) + 'n'   # backslash + n, for C string literals


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    # zanbw/tools/cppbed -> zanbw -> OpenBW; the openbw repo sits next to zanbw
    openbw = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(here))), 'openbw')
    src_path = os.path.join(openbw, 'bwgame.h')
    out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'openbw_traced.h')

    with open(src_path, encoding='utf-8') as f:
        lines = f.read().split('\n')

    out = []
    for i, ln in enumerate(lines, 1):
        st = ln.strip()
        ind = ln[:len(ln) - len(ln.lstrip())]
        if st == 'w.neighbors.push_back({pos, flags, is_goal});':
            out.append(ind + 'if (g_trace) printf("  ADD %d,%d flags=0x%x goal=%d' + NL
                         + '", pos.x, pos.y, flags, is_goal ? 1 : 0);')
            out.append(ln)
        elif st == 'else w.neighbors.push_back({pos, flags, is_goal});':
            out.append(ind + 'else {')
            out.append(ind + chr(9) + 'if (g_trace) printf("  ADD %d,%d flags=0x%x goal=%d'
                         + NL + '", pos.x, pos.y, flags, is_goal ? 1 : 0);')
            out.append(ind + chr(9) + 'w.neighbors.push_back({pos, flags, is_goal});')
            out.append(ind + '}')
        elif st == 'w.neighbors.push_back({w.target, 0xff, true});':
            out.append(ind + 'if (g_trace) printf("  GOALAREA %d,%d' + NL + '", w.target.x, w.target.y);')
            out.append(ln)
        elif st == 'binary_heap_down(open.begin(), open.begin(), open.end(), cmp_node());':
            out.append(ln)
            if 10340 < i < 10380:
                out.append(ind + 'if (g_trace) printf("STEP %d pop pos=%d,%d total=%d rem=%d'
                             ' flags=0x%x region=%zu open=%zu nodes=%zu' + NL + '", g_step++,'
                             ' cur->pos.x, cur->pos.y, (int)cur->total_cost.raw_value,'
                             ' (int)cur->estimated_remaining_cost.raw_value, cur->directional_flags,'
                             ' cur->region->index, open.size(), all_nodes.size());')
        elif st == 'if (add_neighbors(dir)) break;':
            out.append(ind + 'if (g_trace) printf("  DIR %d' + NL + '", dir);')
            out.append(ln)
        elif st == 'if (v.is_goal) {' and 10380 < i < 10450:
            out.append(ind + 'if (g_trace) printf("  NB %d,%d flags=0x%x goal=%d' + NL
                         + '", v.pos.x, v.pos.y, v.flags, v.is_goal ? 1 : 0);')
            out.append(ln)
        else:
            out.append(ln)

    src = '\n'.join(out)
    src = src.replace('namespace bwgame {',
                      'namespace bwgame {\n\nstatic bool g_trace = false;\nstatic int g_step = 0;\n', 1)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(src)
    print('wrote ' + out_path)


if __name__ == '__main__':
    main()
