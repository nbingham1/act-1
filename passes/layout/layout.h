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
#ifndef __ACT_PASS_LAYOUT_H__
#define __ACT_PASS_LAYOUT_H__

#include <set>
#include <map>
#include <act/act.h>
#include <act/iter.h>
#include "hash.h"
#include "bitset.h"
#include "array.h"

#include <act/passes/netlist.h>

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
	A_DECL(act_coord_t, act_coords);
} net_t;

typedef struct act_gate
{
	act_gate();
	act_gate(unsigned int net, unsigned int width, unsigned int length);
	~act_gate();

	unsigned int net;
	unsigned int width;
	unsigned int length;
} act_gate_t;

typedef struct act_dev
{
	act_dev(unsigned int gate, unsigned int source, unsigned int drain, unsigned int bulk, unsigned int width, unsigned int length);
	~act_dev();

	A_DECL(act_gate_t, gate);
	unsigned int source;
	unsigned int drain;
	unsigned int bulk;
} act_dev_t;

typedef struct act_col
{
	unsigned int x;
	unsigned int net;
	unsigned int col;
} act_col_t;

typedef struct layout_task
{
	layout_task();
	~layout_task();

	A_DECL(act_dev_t, pmos);
	A_DECL(act_dev_t, nmos);
	A_DECL(net_t, nets);

	A_DECL(act_col_t, top);
	A_DECL(act_col_t, bot);
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


#endif /* __ACT_PASS_LAYOUT_H__ */
