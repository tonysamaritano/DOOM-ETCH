#include "motion.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int in_bounds(MotionPoint point, unsigned width, unsigned height)
{
    return isfinite(point.x) && isfinite(point.y) && point.x >= 0 && point.y >= 0
        && point.x <= width && point.y <= height;
}

int MotionValid(const MotionPath *path)
{
    if (!path || !path->count || !path->points || !path->kinds ||
        !path->width || !path->height || path->kinds[0] != MOTION_START ||
        !isfinite(path->width_mm) || !isfinite(path->height_mm) ||
        path->width_mm <= 0 || path->height_mm <= 0)
        return 0;
    for (size_t i = 0; i < path->count; i++) {
        if (!in_bounds(path->points[i], path->width, path->height))
            return 0;
        if (i && path->kinds[i] != MOTION_OUTLINE && path->kinds[i] != MOTION_CONNECTOR)
            return 0;
    }
    return 1;
}

void MotionFree(MotionPath *path)
{
    if (path) {
        free(path->points);
        free(path->kinds);
        *path = (MotionPath){0};
    }
}

int MotionCopy(MotionPath *destination, const MotionPath *source)
{
    if (!destination || !source || source->count > SIZE_MAX / sizeof(MotionPoint)
        || !MotionValid(source))
        return 0;
    if (destination == source)
        return 1;
    MotionPath copy = *source;
    size_t bytes = copy.count * sizeof(*copy.points);
    copy.points = malloc(bytes);
    copy.kinds = malloc(copy.count);
    if (!copy.points || !copy.kinds) {
        MotionFree(&copy);
        return 0;
    }
    memcpy(copy.points, source->points, bytes);
    memcpy(copy.kinds, source->kinds, copy.count);
    MotionFree(destination);
    *destination = copy;
    return 1;
}

/* Sorted endpoints share a vertex. Edge pairs always store the smaller ID first. */
typedef struct { MotionPoint point; size_t input; } Endpoint;
typedef struct { size_t a, b; } Edge;
typedef struct {
    Endpoint *vertices;
    Edge *edges;
    size_t vertex_count, edge_count;
    size_t *degree, *offset, *adjacent, *cursor, *stack;
    unsigned char *used;
} Graph;

static void graph_free(Graph *graph)
{
    free(graph->vertices);
    free(graph->edges);
    free(graph->degree);
    free(graph->offset);
    free(graph->adjacent);
    free(graph->cursor);
    free(graph->stack);
    free(graph->used);
}

static int same_point(MotionPoint a, MotionPoint b)
{
    return a.x == b.x && a.y == b.y;
}

static int compare_endpoint(const void *left, const void *right)
{
    MotionPoint a = ((const Endpoint *)left)->point;
    MotionPoint b = ((const Endpoint *)right)->point;
    if (a.x != b.x) return a.x < b.x ? -1 : 1;
    return a.y < b.y ? -1 : a.y > b.y;
}

static int compare_edge(const void *left, const void *right)
{
    const Edge *a = left, *b = right;
    if (a->a != b->a) return a->a < b->a ? -1 : 1;
    return a->b < b->b ? -1 : a->b > b->b;
}

static int graph_vertices(Graph *graph, const MotionSegment *segments, size_t count)
{
    if (!count) return 1;
    size_t endpoints = count * 2;
    graph->vertices = malloc(endpoints * sizeof(*graph->vertices));
    graph->edges = malloc(count * sizeof(*graph->edges));
    size_t *ids = malloc(endpoints * sizeof(*ids));
    if (!graph->vertices || !graph->edges || !ids) { free(ids); return 0; }
    for (size_t i = 0; i < count; i++) {
        graph->vertices[2 * i] = (Endpoint){segments[i].a, 2 * i};
        graph->vertices[2 * i + 1] = (Endpoint){segments[i].b, 2 * i + 1};
    }
    qsort(graph->vertices, endpoints, sizeof(*graph->vertices), compare_endpoint);
    for (size_t i = 0; i < endpoints; i++) {
        Endpoint endpoint = graph->vertices[i];
        size_t n = graph->vertex_count;
        if (!n || !same_point(endpoint.point, graph->vertices[n - 1].point)) {
            graph->vertices[n].point = endpoint.point;
            graph->vertex_count++;
        }
        ids[endpoint.input] = graph->vertex_count - 1;
    }
    for (size_t i = 0; i < count; i++) {
        size_t a = ids[2 * i], b = ids[2 * i + 1];
        graph->edges[i] = a < b ? (Edge){a, b} : (Edge){b, a};
    }
    free(ids);
    return 1;
}

static void graph_unique_edges(Graph *graph, size_t count)
{
    if (!count) return;
    qsort(graph->edges, count, sizeof(*graph->edges), compare_edge);
    for (size_t i = 0; i < count; i++) {
        Edge edge = graph->edges[i];
        size_t n = graph->edge_count;
        if (edge.a != edge.b && (!n || compare_edge(&edge, &graph->edges[n - 1])))
            graph->edges[graph->edge_count++] = edge;
    }
}

static int graph_adjacency(Graph *graph)
{
    size_t vertices = graph->vertex_count, edges = graph->edge_count;
    if (!edges) return 1;
    graph->degree = calloc(vertices, sizeof(*graph->degree));
    graph->offset = calloc(vertices + 1, sizeof(*graph->offset));
    graph->cursor = malloc(vertices * sizeof(*graph->cursor));
    graph->adjacent = malloc(2 * edges * sizeof(*graph->adjacent));
    graph->stack = malloc((edges + 1) * sizeof(*graph->stack));
    graph->used = calloc(edges, sizeof(*graph->used));
    if (!graph->degree || !graph->offset || !graph->cursor || !graph->adjacent
        || !graph->stack || !graph->used) return 0;
    for (size_t i = 0; i < edges; i++) {
        graph->degree[graph->edges[i].a]++;
        graph->degree[graph->edges[i].b]++;
    }
    for (size_t i = 0; i < vertices; i++)
        graph->offset[i + 1] = graph->offset[i] + graph->degree[i];
    memcpy(graph->cursor, graph->offset, vertices * sizeof(*graph->cursor));
    for (size_t i = 0; i < edges; i++) {
        graph->adjacent[graph->cursor[graph->edges[i].a]++] = i;
        graph->adjacent[graph->cursor[graph->edges[i].b]++] = i;
    }
    memcpy(graph->cursor, graph->offset, vertices * sizeof(*graph->cursor));
    return 1;
}

/* Storage is reserved for every DFS step and one connector per component. */
static void append_move(MotionPath *path, MotionPoint point, unsigned char kind)
{
    MotionPoint previous = path->points[path->count - 1];
    if (same_point(previous, point)) return;
    if (path->count >= 2 && kind == MOTION_OUTLINE
        && path->kinds[path->count - 1] == MOTION_OUTLINE) {
        MotionPoint before = path->points[path->count - 2];
        double dx = previous.x - before.x, dy = previous.y - before.y;
        double nx = point.x - previous.x, ny = point.y - previous.y;
        if (fabs(dx * ny - dy * nx) < 1e-9 && dx * nx + dy * ny > 0) {
            path->points[path->count - 1] = point;
            return;
        }
    }
    path->points[path->count] = point;
    path->kinds[path->count++] = kind;
}

/* Buckets of vertex IDs in ascending order, so finding the next stroke checks
 * a few nearby cells instead of every vertex in the scene. */
typedef struct {
    double cell;
    long columns, rows;
    size_t *start, *end, *ids;
} Grid;

static void grid_free(Grid *grid)
{
    free(grid->start);
    free(grid->end);
    free(grid->ids);
}

static long grid_cell(double value, double cell, long cells)
{
    long index = (long)(value / cell);
    return index < 0 ? 0 : index < cells ? index : cells - 1;
}

static size_t grid_bucket(const Grid *grid, MotionPoint point)
{
    return (size_t)(grid_cell(point.y, grid->cell, grid->rows) * grid->columns
        + grid_cell(point.x, grid->cell, grid->columns));
}

static int grid_build(Grid *grid, const Graph *graph, unsigned width, unsigned height)
{
    /* About two vertices per bucket. */
    double buckets = graph->vertex_count / 2.0 + 1;
    grid->cell = fmax(sqrt((double)width * height / buckets), 1e-3);
    grid->columns = (long)ceil(width / grid->cell) + 1;
    grid->rows = (long)ceil(height / grid->cell) + 1;
    size_t cells = (size_t)(grid->columns * grid->rows);
    grid->start = calloc(cells + 1, sizeof(*grid->start));
    grid->end = malloc(cells * sizeof(*grid->end));
    grid->ids = malloc((graph->vertex_count + 1) * sizeof(*grid->ids));
    if (!grid->start || !grid->end || !grid->ids) return 0;
    for (size_t i = 0; i < graph->vertex_count; i++)
        grid->start[grid_bucket(grid, graph->vertices[i].point) + 1]++;
    for (size_t c = 0; c < cells; c++) grid->start[c + 1] += grid->start[c];
    memcpy(grid->end, grid->start, cells * sizeof(*grid->end));
    for (size_t i = 0; i < graph->vertex_count; i++)
        grid->ids[grid->end[grid_bucket(grid, graph->vertices[i].point)]++] = i;
    return 1;
}

/* Scan one bucket and drop vertices with no edges left. Equal distances keep
 * the lowest vertex ID, which is what a full scan in ID order picks. */
static void grid_scan(Grid *grid, const Graph *graph, long x, long y, MotionPoint point,
                      double *best, size_t *closest)
{
    if (x < 0 || y < 0 || x >= grid->columns || y >= grid->rows) return;
    size_t c = (size_t)(y * grid->columns + x), kept = grid->start[c];
    for (size_t k = grid->start[c]; k < grid->end[c]; k++) {
        size_t i = grid->ids[k];
        if (!graph->degree[i]) continue;
        grid->ids[kept++] = i;
        MotionPoint candidate = graph->vertices[i].point;
        double dx = candidate.x - point.x, dy = candidate.y - point.y;
        double distance = dx * dx + dy * dy;
        if (distance < *best || (distance == *best && i < *closest)) {
            *best = distance;
            *closest = i;
        }
    }
    grid->end[c] = kept;
}

static size_t closest_vertex(Grid *grid, const Graph *graph, MotionPoint point)
{
    size_t closest = graph->vertex_count;
    double best = HUGE_VAL;
    long cx = grid_cell(point.x, grid->cell, grid->columns);
    long cy = grid_cell(point.y, grid->cell, grid->rows);
    long rings = grid->columns > grid->rows ? grid->columns : grid->rows;
    for (long r = 0; r <= rings; r++) {
        /* Any vertex in ring r is at least r - 1 whole cells away. */
        double reach = r ? (r - 1) * grid->cell : 0;
        if (closest != graph->vertex_count && reach * reach > best) break;
        for (long x = cx - r; x <= cx + r; x++) {
            grid_scan(grid, graph, x, cy - r, point, &best, &closest);
            if (r) grid_scan(grid, graph, x, cy + r, point, &best, &closest);
        }
        for (long y = cy - r + 1; y <= cy + r - 1; y++) {
            grid_scan(grid, graph, cx - r, y, point, &best, &closest);
            grid_scan(grid, graph, cx + r, y, point, &best, &closest);
        }
    }
    return closest;
}

static void walk_component(Graph *graph, MotionPath *path, size_t root)
{
    size_t depth = 1;
    graph->stack[0] = root;
    while (depth) {
        size_t vertex = graph->stack[depth - 1], next;
        size_t *cursor = &graph->cursor[vertex];
        while (*cursor < graph->offset[vertex + 1] && graph->used[graph->adjacent[*cursor]])
            (*cursor)++;
        if (*cursor == graph->offset[vertex + 1]) {
            depth--;
            if (!depth) continue;
            next = graph->stack[depth - 1];
        } else {
            size_t edge_id = graph->adjacent[(*cursor)++];
            Edge edge = graph->edges[edge_id];
            graph->used[edge_id] = 1;
            next = edge.a == vertex ? edge.b : edge.a;
            graph->degree[vertex]--;
            graph->degree[next]--;
            graph->stack[depth++] = next;
        }
        append_move(path, graph->vertices[next].point, MOTION_OUTLINE);
    }
}

int MotionPlanSegments(MotionPath *path, const MotionSegment *segments, size_t count,
                       unsigned width, unsigned height, MotionPoint start)
{
    /* Match the reference's finite limit and bound every allocation product. */
    if (!path || (!segments && count) || count > 400000 || !width || !height
        || !in_bounds(start, width, height)) return 0;
    for (size_t i = 0; i < count; i++)
        if (!in_bounds(segments[i].a, width, height)
            || !in_bounds(segments[i].b, width, height)) return 0;
    Graph graph = {0};
    Grid grid = {0};
    MotionPath result = {.width = width, .height = height,
        .width_mm = MOTION_WIDTH_MM, .height_mm = MOTION_HEIGHT_MM};
    int success = 0;
    if (!graph_vertices(&graph, segments, count)) goto done;
    graph_unique_edges(&graph, count);
    if (!graph_adjacency(&graph)) goto done;
    size_t capacity = 3 * graph.edge_count + 1;
    result.points = malloc(capacity * sizeof(*result.points));
    result.kinds = malloc(capacity * sizeof(*result.kinds));
    if (!result.points || !result.kinds) goto done;
    result.points[0] = start; result.kinds[0] = MOTION_START; result.count = 1;
    if (graph.edge_count && !grid_build(&grid, &graph, width, height)) goto done;
    while (graph.edge_count) {
        size_t root = closest_vertex(&grid, &graph, result.points[result.count - 1]);
        if (root == graph.vertex_count) break;
        append_move(&result, graph.vertices[root].point, MOTION_CONNECTOR);
        walk_component(&graph, &result, root);
    }
    MotionFree(path);
    *path = result;
    result = (MotionPath){0};
    success = 1;
done:
    grid_free(&grid);
    graph_free(&graph);
    MotionFree(&result);
    return success;
}
