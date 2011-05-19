#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map.hpp"
#include "battle.hpp"
#include "../common/nullpo.hpp"

#define MAX_HEAP 150
struct tmp_path
{
    short x, y, dist, before, cost;
    Direction dir;
    char flag;
};
#define calc_index(x,y) (((x)+(y)*MAX_WALKPATH) & (MAX_WALKPATH*MAX_WALKPATH-1))

/*==========================================
 * 経路探索補助heap push
 *------------------------------------------
 */
static void push_heap_path(int *heap, struct tmp_path *tp, int idx)
{
    int i, h;

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
static void update_heap_path(int *heap, struct tmp_path *tp, int idx)
{
    int i, h;

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
static int pop_heap_path(int *heap, struct tmp_path *tp)
{
    int i, h, k;
    int ret, last;

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
static int calc_cost(struct tmp_path *p, int x_1, int y_1)
{
    int xd, yd;

    nullpo_retr(0, p);

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
static int add_path(int *heap, struct tmp_path *tp, int x, int y, int dist,
                     Direction dir, int before, int x_1, int y_1)
{
    int i;

    nullpo_retr(0, heap);
    nullpo_retr(0, tp);

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
static int can_place(struct map_data *m, int x, int y, int flag)
{
    int c;

    nullpo_retr(0, m);

    c = read_gatp(m, x, y);

    if (c == 1)
        return 0;
    if (!(flag & 0x10000) && c == 5)
        return 0;
    return 1;
}

/*==========================================
 * (x_0,y_0)から(x_1,y_1)へ1歩で移動可能か計算
 *------------------------------------------
 */
static int can_move(struct map_data *m, int x_0, int y_0, int x_1, int y_1,
                     int flag)
{
    nullpo_retr(0, m);

    if (x_0 - x_1 < -1 || x_0 - x_1 > 1 || y_0 - y_1 < -1 || y_0 - y_1 > 1)
        return 0;
    if (x_1 < 0 || y_1 < 0 || x_1 >= m->xs || y_1 >= m->ys)
        return 0;
    if (!can_place(m, x_0, y_0, flag))
        return 0;
    if (!can_place(m, x_1, y_1, flag))
        return 0;
    if (x_0 == x_1 || y_0 == y_1)
        return 1;
    if (!can_place(m, x_0, y_1, flag) || !can_place(m, x_1, y_0, flag))
        return 0;
    return 1;
}

/*==========================================
 * (x_0,y_0)から(dx,dy)方向へcountセル分
 * 吹き飛ばしたあとの座標を所得
 *------------------------------------------
 */
int path_blownpos(int m, int x_0, int y_0, int dx, int dy, int count)
{
    struct map_data *md;

    if (!maps[m].gat)
        return -1;
    md = &maps[m];

    if (count > 15)
    {                           // 最大10マスに制限
        if (battle_config.error_log)
            printf("path_blownpos: count too many %d !\n", count);
        count = 15;
    }
    if (dx > 1 || dx < -1 || dy > 1 || dy < -1)
    {
        if (battle_config.error_log)
            printf("path_blownpos: illeagal dx=%d or dy=%d !\n", dx, dy);
        dx = (dx >= 0) ? 1 : ((dx < 0) ? -1 : 0);
        dy = (dy >= 0) ? 1 : ((dy < 0) ? -1 : 0);
    }

    while ((count--) > 0 && (dx != 0 || dy != 0))
    {
        if (!can_move(md, x_0, y_0, x_0 + dx, y_0 + dy, 0))
        {
            int fx = (dx != 0 && can_move(md, x_0, y_0, x_0 + dx, y_0, 0));
            int fy = (dy != 0 && can_move(md, x_0, y_0, x_0, y_0 + dy, 0));
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
int path_search(struct walkpath_data *wpd, int m, int x_0, int y_0, int x_1,
                 int y_1, int flag)
{
    int heap[MAX_HEAP + 1];
    struct tmp_path tp[MAX_WALKPATH * MAX_WALKPATH];
    int i, rp, x, y;
    struct map_data *md;
    int dx, dy;

    nullpo_retr(0, wpd);

    if (!maps[m].gat)
        return -1;
    md = &maps[m];
    if (x_1 < 0 || x_1 >= md->xs || y_1 < 0 || y_1 >= md->ys
        || (i = read_gatp(md, x_1, y_1)) == 1 || i == 5)
        return -1;

    // easy
    dx = (x_1 - x_0 < 0) ? -1 : 1;
    dy = (y_1 - y_0 < 0) ? -1 : 1;
    for (x = x_0, y = y_0, i = 0; x != x_1 || y != y_1;)
    {
        if (i >= sizeof(wpd->path))
            return -1;
        if (x != x_1 && y != y_1)
        {
            if (!can_move(md, x, y, x + dx, y + dy, flag))
                break;
            x += dx;
            y += dy;
            wpd->path[i++] = (dx < 0)
                ? ((dy > 0) ? DIR_SW : DIR_NW)
                : ((dy < 0) ? DIR_NE : DIR_SE);
        }
        else if (x != x_1)
        {
            if (!can_move(md, x, y, x + dx, y, flag))
                break;
            x += dx;
            wpd->path[i++] = (dx < 0) ? DIR_W : DIR_E;
        }
        else
        {                       // y!=y_1
            if (!can_move(md, x, y, x, y + dy, flag))
                break;
            y += dy;
            wpd->path[i++] = (dy > 0) ? DIR_S : DIR_N;
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
    tp[i].dir = DIR_S;
    tp[i].before = 0;
    tp[i].cost = calc_cost(&tp[i], x_1, y_1);
    tp[i].flag = 0;
    heap[0] = 0;
    push_heap_path(heap, tp, calc_index(x_0, y_0));
    while (1)
    {
        int e = 0;

        if (heap[0] == 0)
            return -1;
        rp = pop_heap_path(heap, tp);
        x = tp[rp].x;
        y = tp[rp].y;
        if (x == x_1 && y == y_1)
        {
            int len, j;

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
        if (can_move(md, x, y, x + 1, y - 1, flag))
            e += add_path(heap, tp, x + 1, y - 1, tp[rp].dist + 14, DIR_NE, rp,
                           x_1, y_1);
        if (can_move(md, x, y, x + 1, y, flag))
            e += add_path(heap, tp, x + 1, y, tp[rp].dist + 10, DIR_E, rp, x_1,
                           y_1);
        if (can_move(md, x, y, x + 1, y + 1, flag))
            e += add_path(heap, tp, x + 1, y + 1, tp[rp].dist + 14, DIR_SE, rp,
                           x_1, y_1);
        if (can_move(md, x, y, x, y + 1, flag))
            e += add_path(heap, tp, x, y + 1, tp[rp].dist + 10, DIR_S, rp, x_1,
                           y_1);
        if (can_move(md, x, y, x - 1, y + 1, flag))
            e += add_path(heap, tp, x - 1, y + 1, tp[rp].dist + 14, DIR_SW, rp,
                           x_1, y_1);
        if (can_move(md, x, y, x - 1, y, flag))
            e += add_path(heap, tp, x - 1, y, tp[rp].dist + 10, DIR_W, rp, x_1,
                           y_1);
        if (can_move(md, x, y, x - 1, y - 1, flag))
            e += add_path(heap, tp, x - 1, y - 1, tp[rp].dist + 14, DIR_NW, rp,
                           x_1, y_1);
        if (can_move(md, x, y, x, y - 1, flag))
            e += add_path(heap, tp, x, y - 1, tp[rp].dist + 10, DIR_W, rp, x_1,
                           y_1);
        tp[rp].flag = 1;
        if (e || heap[0] >= MAX_HEAP - 5)
            return -1;
    }
    return -1;
}
