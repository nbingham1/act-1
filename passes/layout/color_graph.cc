#include "color_graph.h"

#include "bitset.h"

color_edge::color_edge()
{
	x = 0;
	y = 0;
}

color_edge::color_edge(unsigned int x, unsigned int y)
{
	this->x = x;
	this->y = y;
}

color_edge::~color_edge()
{
}

color_graph::color_graph(unsigned int count)
{
	A_INIT(edges);
	A_INIT(color);
	if (count > 0) {
		A_NEWP(color, unsigned int, count);
		for (int i = 0; i < count; i++) {
			A_NEXT(color) = 0;
			A_INC(color);
		}
	}

	stage[0] = 0;
	stage[1] = 0;
}

color_graph::~color_graph()
{
	A_FREE(edges);
	A_FREE(color);
}

void color_graph::init(unsigned int count)
{
	if (count > A_LEN(color)) {
		A_NEWP(color, unsigned int, (count - A_LEN(color)));
		for (int i = A_LEN(color); i < count; i++) {
			A_NEXT(color) = 0;
			A_INC(color);
		}
	}
}

bool color_graph::has_edge(unsigned int a, unsigned int b)
{
	for (int i = 0; i < stage[0]; i++) {
		if ((edges[i].x == a and edges[i].y == b) or (edges[i].x == b and edges[i].y == a)) {
			return true;
		}
	}
	for (int i = stage[1]; i < A_LEN(edges); i++) {
		if ((edges[i].x == a and edges[i].y == b) or (edges[i].x == b and edges[i].y == a)) {
			return true;
		}
	}

	return false;
}

void color_graph::push_edge(unsigned int a, unsigned int b)
{
	A_NEW(edges, color_edge_t);
	A_NEXT(edges) = color_edge_t(a, b);
	A_INC(edges);


	if (color[a] == color[b]) {
		if (a >= A_LEN(color) or b >= A_LEN(color)) {
			// this should never happen if this structure is properly initialized
			int len = (a > b ? a : b) - A_LEN(color);
			A_NEWP(color, unsigned int, len);
			for (int i = 0; i < len; i++) {
				A_NEXT(color) = 0;
				A_INC(color);
			}
		}

		// TODO: bug rajit to use constructors and destructors in the bitset so
		// this can be a static variable and we no longer pay the allocation costs
		// on every call
		bitset *s = bitset_new(A_LEN(color));

		for (int i = 0; i < stage[0]; i++) {
			if (edges[i].x == b) {
				bitset_set(s, color[edges[i].y]);
			} else if (edges[i].y == b) {
				bitset_set(s, color[edges[i].x]);
			}
		}

		for (int i = stage[1]; i < A_LEN(edges); i++) {
			if (edges[i].x == b) {
				bitset_set(s, color[edges[i].y]);
			} else if (edges[i].y == b) {
				bitset_set(s, color[edges[i].x]);
			}
		}
		
		for (int i = 0; i < A_LEN(color); i++) {
			if (bitset_tst(s, i) == 0) {
				color[b] = i;
				break;
			}
		}

		bitset_free(s);
	}
}

unsigned int color_graph::stash_cost()
{
	return A_LEN(edges) - stage[1];
}

unsigned int color_graph::stage_cost()
{
	return stage[1] - stage[0];
}

void color_graph::stash()
{
	A_DELETE(edges, stage[0], stage[1] - stage[0]);
	stage[1] = A_LEN(edges);
}

void color_graph::commit()
{
	A_DELETE(edges, stage[1], A_LEN(edges)-stage[1]);
	stage[0] = A_LEN(edges);
	stage[1] = A_LEN(edges);
}

void color_graph::clear()
{
	A_LEN(edges) = stage[1];
}

void color_graph::reset()
{
	A_LEN(edges) = stage[0];
	stage[1] = stage[0];
}

