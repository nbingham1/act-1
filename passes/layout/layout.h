/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */

#pragma once

#include <act/act.h>
#include <act/iter.h>
#include "hash.h"
#include "bitset.h"
#include "array.h"
#include "color_graph.h"

#include <act/passes/netlist.h>

struct layout_task;

typedef struct act_coord
{
	act_coord();
	act_coord(unsigned int x, unsigned int y);
	~act_coord();

	unsigned int x, y;
} act_coord_t;

typedef struct net
{
	net();
	~net();

	node_t *node;

	// the total number of ports for this net in the cell
	// a port is a connection of the net to the source, drain, or gate of a transistor
	unsigned int ports;
} net_t;

typedef struct act_gate
{
	act_gate();
	act_gate(unsigned int net, unsigned int width, unsigned int length);
	~act_gate();

	// the id of the net for this gate
	// this indexes into task->nets or task->stack[i].ovr
	unsigned int net;

	// transistor dimensions
	unsigned int width;
	unsigned int length;
} act_gate_t;

typedef struct act_dev
{
	act_dev(unsigned int gate, unsigned int source, unsigned int drain, unsigned int bulk, unsigned int width, unsigned int length);
	~act_dev();

	A_DECL(act_gate_t, gate);
	// the id of the net for this stack
	// this indexes into task->nets or task->stack[i].ovr
	unsigned int source;
	unsigned int drain;
	unsigned int bulk;

	// whether or not this stack has been placed in the layout problem
	unsigned int selected:1;
} act_dev_t;

typedef struct act_dev_sel
{
	act_dev_sel();
	act_dev_sel(unsigned int idx, unsigned int flip);
	~act_dev_sel();

	// the device stack stack index
	unsigned int idx;

	// unflipped is source on left, drain on right. Flipped is drain on left,
	// source on right.
	unsigned int flip:1;
} act_dev_sel_t;

typedef struct act_col
{
	act_col();
	act_col(unsigned int pos, unsigned int net);
	~act_col();

	// position in layout of this column (columns aren't uniformly spaced)
	unsigned int pos;

	// the net connected to this port
	// this indexes into task->nets or task->stack[i].ovr
	unsigned int net;
} act_col_t;

typedef struct act_rect
{
	unsigned int left;
	unsigned int right;
	unsigned int bottom;
	unsigned int top;
	unsigned int layer;
} act_rect_t;


typedef struct act_ovr
{
	act_ovr();
	~act_ovr();

	// the number of ports this net has within this stack
	int gates;
	int links;

	// the latest port id that was routed
	int gate_idx;
	int link_idx;
} act_ovr_t;

typedef struct act_stack
{
	act_stack();
	~act_stack();

	A_DECL(act_dev_t, mos);
	A_DECL(act_dev_sel_t, sel);
	A_DECL(act_col_t, col);
	A_DECL(act_ovr_t, ovr);
	color_graph_t layer;

	unsigned int stage[2];
	unsigned int idx[2];
	unsigned int flip[2];

	void init(unsigned int nets);

	void stash();
	void commit();
	void clear();
	void reset();

	void print(const char *dev);
	void count_ports();
	void collect(layout_task *task);
	void stage_col(int net, bool is_gate);
	int stage_stack(int sel, int flip);
} act_stack_t;

typedef struct act_route
{
	A_DECL(int, assign);
} act_route_t;

typedef struct layout_task
{
	layout_task();
	~layout_task();

	act_stack_t stack[2];
	A_DECL(net_t, nets);

	A_DECL(act_route_t, cols);
	unsigned int stage[2];

	A_DECL(act_rect_t, geo);
	
	void stash();
	void commit();
	void clear();
	void reset();
	void stage_channel();
} layout_task_t;

class ActLayoutPass : public ActPass {
public:
  ActLayoutPass (Act *a);
  ~ActLayoutPass ();

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

	ActNetlistPass *np;

	double lambda;
	int n_fold;
	int p_fold;
	int discrete_len;

	void collect_stacks(layout_task *task);
	void process_cell(Process *p);
};

