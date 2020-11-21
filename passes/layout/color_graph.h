#pragma once

#include "array.h"

typedef struct color_edge
{
	color_edge();
	color_edge(unsigned int x, unsigned int y);
	~color_edge();

	unsigned int x;
	unsigned int y;
} color_edge_t;

typedef struct color_graph
{
	color_graph(unsigned int count = 0);
	~color_graph();

	A_DECL(color_edge_t, edges);
	A_DECL(int, color);
	unsigned int stage[2];

	void init(unsigned int count = 0);

	bool has_edge(unsigned int a, unsigned int b);
	void push_edge(unsigned int a, unsigned int b);

	unsigned int stash_cost();
	unsigned int stage_cost();

	void stash();
	void commit();
	void clear();
	void reset();
} color_graph_t;

