#include "path.hpp"

#include "../common/nullpo.hpp"

#include "main.hpp"

#define MAX_HEAP 150

struct tmp_path
{
    sint16 x, y, dist, before, cost;
    Direction dir;
    char flag;
};

#define calc_index(x, y) (((x) + (y) * MAX_WALKPATH) & (MAX_WALKPATH * MAX_WALKPATH - 1))

/*==========================================
 * 経路探索補助heap push
 *------------------------------------------
 */
static void push_heap_path(sint32 *heap, struct tmp_path *tp, sint32 idx)
{
    sint32 i, h;

    if (heap == NULL || tp == NULL)
    {
        printf("push_heap_path nullpo\n");
        return;
    }

    heap[0]++;

    for (h = heap[0] - 1, i = (h - 1) / 2;
         h > 0 && tp[idx].cost < tp[heap[i + 1]].cost; i = (h - 1) / 2)
        heap[h + 1] = heap[i + 1], h = i;
    heap[h + 1] = idx;
}

/*==========================================
 * 経路探索補助heap update
 * costが減ったので根の方へ移動
 *------------------------------------------
 */
static void update_heap_path(sint32 *heap, struct tmp_path *tp, sint32 idx)
{
    sint32 i, h;

    nullpo_retv(heap);
    nullpo_retv(tp);

    for (h = 0; h < heap[0]; h++)
        if (heap[h + 1] == idx)
            break;
    if (h == heap[0])
    {
        fprintf(stderr, "update_heap_path bug\n");
        exit(1);
    }
    for (i = (h - 1) / 2;
         h > 0 && tp[idx].cost < tp[heap[i + 1]].cost; i = (h - 1) / 2)
        heap[h + 1] = heap[i + 1], h = i;
    heap[h + 1] = idx;
}

/*==========================================
 * 経路探索補助heap pop
 *------------------------------------------
 */
static sint32 pop_heap_path(sint32 *heap, struct tmp_path *tp)
{
    sint32 i, h, k;
    sint32 ret, last;

    nullpo_retr(-1, heap);
    nullpo_retr(-1, tp);

    if (heap[0] <= 0)
        return -1;
    ret = heap[1];
    last = heap[heap[0]];
    heap[0]--;

    for (h = 0, k = 2; k < heap[0]; k = k * 2 + 2)
    {
        if (tp[heap[k + 1]].cost > tp[heap[k]].cost)
            k--;
        heap[h + 1] = heap[k + 1], h = k;
    }
    if (k == heap[0])
        heap[h + 1] = heap[k], h = k - 1;

    for (i = (h - 1) / 2;
         h > 0 && tp[heap[i + 1]].cost > tp[last].cost; i = (h - 1) / 2)
        heap[h + 1] = heap[i + 1], h = i;
    heap[h + 1] = last;

    return ret;
}

/*==========================================
 * 現在の点のcost計算
 *------------------------------------------
 */
static sint32 calc_cost(struct tmp_path *p, sint32 x_1, sint32 y_1)
{
    sint32 xd, yd;

    nullpo_ret(p);

    xd = x_1 - p->x;
    if (xd < 0)
        xd = -xd;
    yd = y_1 - p->y;
    if (yd < 0)
        yd = -yd;
    return (xd + yd) * 10 + p->dist;
}

/*==========================================
 * 必要ならpathを追加/修正する
 *------------------------------------------
 */
static sint32 add_path(sint32 *heap, struct tmp_path *tp, sint32 x, sint32 y, sint32 dist,
                     Direction dir, sint32 before, sint32 x_1, sint32 y_1)
{
    sint32 i;

    nullpo_ret(heap);
    nullpo_ret(tp);

    i = calc_index(x, y);

    if (tp[i].x == x && tp[i].y == y)
    {
        if (tp[i].dist > dist)
        {
            tp[i].dist = dist;
            tp[i].dir = dir;
            tp[i].before = before;
            tp[i].cost = calc_cost(&tp[i], x_1, y_1);
            if (tp[i].flag)
                push_heap_path(heap, tp, i);
            else
                update_heap_path(heap, tp, i);
            tp[i].flag = 0;
        }
        return 0;
    }

    if (tp[i].x || tp[i].y)
        return 1;

    tp[i].x = x;
    tp[i].y = y;
    tp[i].dist = dist;
    tp[i].dir = dir;
    tp[i].before = before;
    tp[i].cost = calc_cost(&tp[i], x_1, y_1);
    tp[i].flag = 0;
    push_heap_path(heap, tp, i);

    return 0;
}

/*==========================================
 * (x,y)が移動不可能地帯かどうか
 * flag 0x10000 遠距離攻撃判定
 *------------------------------------------
 */
static sint32 can_place(map_data_local *m, sint32 x, sint32 y)
{
    nullpo_ret(m);

    MapCell c = read_gatp(m, x, y);

    if (c & MapCell::SOLID)
        return 0;
    return 1;
}

/*==========================================
 * (x_0,y_0)から(x_1,y_1)へ1歩で移動可能か計算
 *------------------------------------------
 */
static sint32 can_move(map_data_local *m, sint32 x_0, sint32 y_0, sint32 x_1, sint32 y_1)
{
    nullpo_ret(m);

    if (x_0 - x_1 < -1 || x_0 - x_1 > 1 || y_0 - y_1 < -1 || y_0 - y_1 > 1)
        return 0;
    if (x_1 < 0 || y_1 < 0 || x_1 >= m->xs || y_1 >= m->ys)
        return 0;
    if (!can_place(m, x_0, y_0))
        return 0;
    if (!can_place(m, x_1, y_1))
        return 0;
    if (x_0 == x_1 || y_0 == y_1)
        return 1;
    if (!can_place(m, x_0, y_1) || !can_place(m, x_1, y_0))
        return 0;
    return 1;
}

/*==========================================
 * (x_0,y_0)から(dx,dy)方向へcountセル分
 * 吹き飛ばしたあとの座標を所得
 *------------------------------------------
 */
sint32 path_blownpos(sint32 m, sint32 x_0, sint32 y_0, sint32 dx, sint32 dy, sint32 count)
{
    if (!maps[m].gat)
        return -1;
    map_data_local *md = &maps[m];

    if (count > 15)
    {                           // 最大10マスに制限
        map_log("path_bmap_loglownpos: count too many %d !\n", count);
        count = 15;
    }
    if (dx > 1 || dx < -1 || dy > 1 || dy < -1)
    {
        map_log("path_bmap_loglownpos: illeagal dx=%d or dy=%d !\n", dx, dy);
        dx = (dx >= 0) ? 1 : ((dx < 0) ? -1 : 0);
        dy = (dy >= 0) ? 1 : ((dy < 0) ? -1 : 0);
    }

    while ((count--) > 0 && (dx != 0 || dy != 0))
    {
        if (!can_move(md, x_0, y_0, x_0 + dx, y_0 + dy))
        {
            sint32 fx = (dx != 0 && can_move(md, x_0, y_0, x_0 + dx, y_0));
            sint32 fy = (dy != 0 && can_move(md, x_0, y_0, x_0, y_0 + dy));
            if (fx && fy)
            {
                if (rand() & 1)
                    dx = 0;
                else
                    dy = 0;
            }
            if (!fx)
                dx = 0;
            if (!fy)
                dy = 0;
        }
        x_0 += dx;
        y_0 += dy;
    }
    return (x_0 << 16) | y_0;
}

/*==========================================
 * path探索 (x_0,y_0)->(x_1,y_1)
 *------------------------------------------
 */
sint32 path_search(struct walkpath_data *wpd, sint32 m, sint32 x_0, sint32 y_0, sint32 x_1,
                 sint32 y_1, sint32 flag)
{
    sint32 heap[MAX_HEAP + 1];
    struct tmp_path tp[MAX_WALKPATH * MAX_WALKPATH];
    sint32 rp, x, y;
    sint32 dx, dy;

    nullpo_ret(wpd);

    if (!maps[m].gat)
        return -1;
    map_data_local *md = &maps[m];
    if (x_1 < 0 || x_1 >= md->xs || y_1 < 0 || y_1 >= md->ys
        || (read_gatp(md, x_1, y_1) & MapCell::SOLID))
        return -1;

    // easy
    dx = (x_1 - x_0 < 0) ? -1 : 1;
    dy = (y_1 - y_0 < 0) ? -1 : 1;
    sint32 i;
    for (x = x_0, y = y_0, i = 0; x != x_1 || y != y_1; )
    {
        if (i >= sizeof(wpd->path))
            return -1;
        if (x != x_1 && y != y_1)
        {
            if (!can_move(md, x, y, x + dx, y + dy))
                break;
            x += dx;
            y += dy;
            wpd->path[i++] = (dx < 0)
                ? ((dy > 0) ? Direction::SW : Direction::NW)
                : ((dy < 0) ? Direction::NE : Direction::SE);
        }
        else if (x != x_1)
        {
            if (!can_move(md, x, y, x + dx, y))
                break;
            x += dx;
            wpd->path[i++] = (dx < 0) ? Direction::W : Direction::E;
        }
        else
        {                       // y!=y_1
            if (!can_move(md, x, y, x, y + dy))
                break;
            y += dy;
            wpd->path[i++] = (dy > 0) ? Direction::S : Direction::N;
        }
        if (x == x_1 && y == y_1)
        {
            wpd->path_len = i;
            wpd->path_pos = 0;
            wpd->path_half = 0;
            return 0;
        }
    }
    if (flag & 1)
        return -1;

    memset(tp, 0, sizeof(tp));

    i = calc_index(x_0, y_0);
    tp[i].x = x_0;
    tp[i].y = y_0;
    tp[i].dist = 0;
    tp[i].dir = Direction::S;
    tp[i].before = 0;
    tp[i].cost = calc_cost(&tp[i], x_1, y_1);
    tp[i].flag = 0;
    heap[0] = 0;
    push_heap_path(heap, tp, calc_index(x_0, y_0));
    while (1)
    {
        sint32 e = 0;

        if (heap[0] == 0)
            return -1;
        rp = pop_heap_path(heap, tp);
        x = tp[rp].x;
        y = tp[rp].y;
        if (x == x_1 && y == y_1)
        {
            sint32 len, j;

            for (len = 0, i = rp; len < 100 && i != calc_index(x_0, y_0);
                 i = tp[i].before, len++);
            if (len == 100 || len >= sizeof(wpd->path))
                return -1;
            wpd->path_len = len;
            wpd->path_pos = 0;
            wpd->path_half = 0;
            for (i = rp, j = len - 1; j >= 0; i = tp[i].before, j--)
                wpd->path[j] = tp[i].dir;

            return 0;
        }
        if (can_move(md, x, y, x + 1, y - 1))
            e += add_path(heap, tp, x + 1, y - 1, tp[rp].dist + 14, Direction::NE, rp,
                           x_1, y_1);
        if (can_move(md, x, y, x + 1, y))
            e += add_path(heap, tp, x + 1, y, tp[rp].dist + 10, Direction::E, rp, x_1,
                           y_1);
        if (can_move(md, x, y, x + 1, y + 1))
            e += add_path(heap, tp, x + 1, y + 1, tp[rp].dist + 14, Direction::SE, rp,
                           x_1, y_1);
        if (can_move(md, x, y, x, y + 1))
            e += add_path(heap, tp, x, y + 1, tp[rp].dist + 10, Direction::S, rp, x_1,
                           y_1);
        if (can_move(md, x, y, x - 1, y + 1))
            e += add_path(heap, tp, x - 1, y + 1, tp[rp].dist + 14, Direction::SW, rp,
                           x_1, y_1);
        if (can_move(md, x, y, x - 1, y))
            e += add_path(heap, tp, x - 1, y, tp[rp].dist + 10, Direction::W, rp, x_1,
                           y_1);
        if (can_move(md, x, y, x - 1, y - 1))
            e += add_path(heap, tp, x - 1, y - 1, tp[rp].dist + 14, Direction::NW, rp,
                           x_1, y_1);
        if (can_move(md, x, y, x, y - 1))
            e += add_path(heap, tp, x, y - 1, tp[rp].dist + 10, Direction::W, rp, x_1,
                           y_1);
        tp[rp].flag = 1;
        if (e || heap[0] >= MAX_HEAP - 5)
            return -1;
    }
    return -1;
}
