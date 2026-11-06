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
