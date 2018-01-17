#!/usr/bin/env python3
from sys import stderr


def popcount(x):
    return bin(x).count('1')


def remove_prefix(text, prefix):
    if text.startswith(prefix):
        return text[len(prefix):]
    return text


def index(what, where):
    try:
        return where.index(what)
    except ValueError:
        return -1


def main():
    max_triangles = 4
    print('// CODE BELOW IS GENERATED')
    print()
    print('constant int MAX_TRIANGLES = ', max_triangles, ';', sep='')
    print()

    vertices = 8
    positions = [
        [-1 + 2 * (popcount(i & (1 << j)) != 0) for j in range(3)] for i in range(vertices)
    ]

    adjustment = [
        {j for j in range(vertices) if popcount(i ^ j) == 1} for i in range(vertices)
    ]

    edges = [
        [i, j] for i in range(vertices) for j in adjustment[i]
    ]
    for e in edges:
        e.sort()
    edges = sorted(set(map(tuple, edges)))
    '''
       .--11---.
      /|      /|
     9 6    10 7
    .--8----.  |
    |  |    |  |
    2  |    4  |
    |  .--5-|--.
    | /     | /
    |1      |3
    .--0----.
    '''

    def edge_id(a, b):
        return edges.index((min(a, b), max(a, b)))

    print('int vertex_by_edge(')
    print('\tint n, int m, int k,')
    print('\tint point_id,')
    print('\tint id, global read_only int vertex_ids[]')
    print(') {')
    print('\tint v_line = n + 1;')
    print('\tint v_plane = v_line * (m + 1);')
    print('\tswitch (id) {')
    for i, e in enumerate(edges):
        add = ''
        final_add = ''
        if positions[e[0]][0] != positions[e[1]][0]:  # x edge
            final_add += ' + 0'
        elif positions[e[0]][1] != positions[e[1]][1]:  # y edge
            final_add += ' + 1'
        else:  # z edge
            final_add += ' + 2'
        if positions[e[0]][2] == 1:
            add += ' + v_plane'
        if positions[e[0]][1] == 1:
            add += ' + v_line'
        if positions[e[0]][0] == 1:
            add += ' + 1'
        print('\tcase ', '%2d' % i, ': return vertex_ids[(point_id', add, ') * 3', final_add, ']; // ', e, sep='')
    print('\t}')
    print('\treturn -3;')
    print('}')
    print()

    def plane_id(a, b, c):
        '''
               5     3
            .-------.
           /|      /|
          / |     / |
         .-------.  |
         |  |    |  |
        0|  |    |  |1
         |  .----|--.
         | /     | /
         |/      |/
         .-------.
        2     4
        '''
        anded = a & b & c
        ored = a | b | c
        if (ored & 1) == 0:
            return 0
        if (anded & 1) == 1:
            return 1
        if (ored & 2) == 0:
            return 2
        if (anded & 2) == 2:
            return 3
        if (ored & 4) == 0:
            return 4
        if (anded & 4) == 4:
            return 5
        return -1

    def case_unrelated(debug, *vs):
        if debug:
            print('\t//   ', len(vs), ' triangle(s) around ', vs, ', unrelated', sep='')
        return [edge_id(v, a) for v in vs for a in adjustment[v]]

    def case2(v1, v2, debug):
        if debug:
            print('\t//   ', '2 triangles around ', v1, ' and ', v2, ', adjustment', sep='')
        a, b = [edge_id(v1, i) for i in adjustment[v1] if i != v2]
        c, d = [edge_id(v2, i) for i in adjustment[v2] if i != v1]
        return [a, b, c, b, c, d]

    def case4(v1, v2, v3, debug):
        if debug:
            print('\t//   ', '3 triangles around ', v1, ', ', v2, ' and ', v3, ', one plane', sep='')
        vs = (v1, v2, v3)
        m = max(vs, key=lambda t: sum([1 if i in vs else 0 for i in adjustment[t]]))
        l, r = [a for a in (v1, v2, v3) if a != m]
        mt = edge_id(m, [a for a in adjustment[m] if a not in vs][0])
        o = [a for a in adjustment[l] if a in adjustment[r] and not a == m][0]
        lt = edge_id(l, [a for a in adjustment[l] if a not in (m, o)][0])
        rt = edge_id(r, [a for a in adjustment[r] if a not in (m, o)][0])
        lo = edge_id(l, o)
        ro = edge_id(r, o)
        return [lt, rt, mt, lt, rt, lo, rt, lo, ro]

    def case5(v1, v2, v3, v4, debug):
        if debug:
            print('\t//   ', '2 triangles around ', v1, ', ', v2, ', ', v3, ' and ', v4, ', one plane', sep='')
        ov3 = [a for a in (v1, v2, v3, v4) if a not in adjustment[v1] and a != v1][0]
        ov2, ov4 = [a for a in (v2, v3, v4) if a != ov3]
        v1, v2, v3, v4 = v1, ov2, ov3, ov4
        v1t = [edge_id(v1, a) for a in adjustment[v1] if a not in (v2, v4)][0]
        v2t = [edge_id(v2, a) for a in adjustment[v2] if a not in (v3, v1)][0]
        v3t = [edge_id(v3, a) for a in adjustment[v3] if a not in (v4, v2)][0]
        v4t = [edge_id(v4, a) for a in adjustment[v4] if a not in (v1, v3)][0]
        return [v1t, v2t, v4t, v2t, v4t, v3t]

    def case6(v1, debug):
        if debug:
            print('\t//   ', '4 triangles around ', v1, ', star', sep='')
        return {
            0: [4, 9, 8, 4, 6, 9, 3, 6, 4, 3, 5, 6],
            1: [7, 8, 10, 7, 2, 8, 5, 2, 7, 5, 1, 2],
            2: [2, 11, 9, 2, 7, 11, 0, 7, 2, 0, 3, 7],
            3: [6, 10, 11, 6, 4, 10, 1, 4, 6, 1, 0, 4],
            4: [4, 11, 10, 4, 6, 11, 0, 6, 4, 0, 1, 6],
            5: [7, 9, 11, 7, 2, 9, 3, 2, 7, 3, 0, 2],
            6: [2, 10, 8, 2, 7, 10, 1, 7, 2, 1, 5, 7],
            7: [6, 8, 9, 6, 4, 8, 5, 4, 6, 5, 3, 4]
        }[v1]

    lens = []
    print_debug = True
    unhandled = 0
    print('constant char edges[', max_triangles * 3 << vertices, '] = {', sep='')
    for case in range(1 << len(positions)):
        case_bin = [(case & (1 << j)) != 0 for j in range(vertices)]
        count = case_bin.count(True)
        inverted = False
        if count > 4:
            inverted = True
            case_bin = [not x for x in case_bin]
            count = case_bin.count(True)
        edges_in_case = []
        if print_debug:
            print('\t// case ', case, ', count is ', count, sep='')
            if inverted:
                print('\t// (inverted) ', end='\n')
        vs = [i for i, x in enumerate(case_bin) if case_bin[i]]

        ws = []
        for v in vs:
            is_related = False
            for e in adjustment[v]:
                if case_bin[e]:
                    is_related = True
            if not is_related:
                ws.append(v)
        if len(ws) > 0:
            edges_in_case += case_unrelated(print_debug, *ws)
            vs = [i for i in vs if i not in ws]
            count = len(vs)

        ws = []
        for i in vs:
            for j in vs:
                if i >= j:
                    continue
                if i not in adjustment[j]:
                    continue
                if len([a for a in adjustment[i] if a in vs]) != 1:
                    continue
                if len([a for a in adjustment[j] if a in vs]) != 1:
                    continue
                ws += [i, j]
                edges_in_case += case2(i, j, print_debug)

        if len(ws) > 0:
            vs = [i for i in vs if i not in ws]
            count = len(vs)

        if count == 0:
            pass
        elif count == 1:
            assert False
        elif count == 2:
            assert False
        elif count == 3:
            plane = plane_id(vs[0], vs[1], vs[2])
            assert plane != -1
            edges_in_case += case4(vs[0], vs[1], vs[2], print_debug)
        else:
            assert count == 4
            plane1 = plane_id(vs[0], vs[1], vs[2])
            plane2 = plane_id(vs[1], vs[2], vs[3])
            if plane1 == plane2 != -1:
                edges_in_case += case5(vs[0], vs[1], vs[2], vs[3], print_debug)
            else:
                stars = [v for v in vs if len([u for u in adjustment[v] if u in vs]) == 3]
                if len(stars) == 1:
                    edges_in_case += case6(stars[0], print_debug)
                else:
                    if print_debug:
                        print('\t// UNHANDLED 4', vs)
                        unhandled += 1

        assert len(edges_in_case) % 3 == 0
        assert len(edges_in_case) // 3 <= max_triangles
        lens.append(len(edges_in_case) // 3)
        edges_in_case += [-1 for _ in range(3 * max_triangles - len(edges_in_case))]
        print('\t', ', '.join(map(lambda t: '%2d' % t, edges_in_case)), ',', sep='')
    print('};')
    if print_debug:
        print('// unhandled: ', unhandled)
    print()

    print('constant char case_sizes[', 1 << vertices,'] = {\n\t', sep='', end='')
    print(*lens, sep=', ')
    print('};')

    print()
    print('// CODE ABOVE IS GENERATED')
    print()

    print('#pragma once', file=stderr)
    print('// CODE BELOW IS GENERATED', file=stderr)
    print(file=stderr)
    print('#define MAX_TRIANGLES ', max_triangles, file=stderr)
    print(file=stderr)
    print('// CODE ABOVE IS GENERATED', file=stderr)

if __name__ == '__main__':
    main()
