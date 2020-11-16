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
#include <stdio.h>
#include <string.h>
#include <set>
#include <map>
#include <utility>
#include <act/passes/layout.h>
#include <act/passes/sizing.h>
#include <act/passes/booleanize.h>
#include <act/passes/netlist.h>
#include <config.h>
#include <stdint.h>

act_coord::act_coord()
{
}

act_coord::act_coord(unsigned int x, unsigned int y)
{
	this->x = x;
	this->y = y;
}

act_coord::~act_coord()
{
}

net::net()
{
	node = NULL;
	ports = 0;
}

net::~net()
{
}

act_gate::act_gate()
{
}

act_gate::act_gate(unsigned int net, unsigned int width, unsigned int length)
{
	this->net = net;
	this->width = width;
	this->length = length;
}

act_gate::~act_gate()
{
}


act_dev::act_dev(unsigned int gate, unsigned int source, unsigned int drain, unsigned int bulk, unsigned int width, unsigned int length)
{
	A_INIT(this->gate);
	A_NEW(this->gate, act_gate_t);
	A_NEXT(this->gate) = act_gate_t(gate, width, length);
	A_INC(this->gate);
	this->source = source;
	this->drain = drain;
	this->bulk = bulk;

	this->selected = 0;
}

act_dev::~act_dev()
{
	A_FREE(this->gate);
}

act_dev_sel::act_dev_sel()
{
	idx = 0;
	flip = 0;
}

act_dev_sel::act_dev_sel(unsigned int idx, unsigned int flip)
{
	this->idx = idx;
	this->flip = flip;
}

act_dev_sel::~act_dev_sel()
{
}

act_col::act_col()
{
	this->pos = 0;
	this->net = 0;
	this->idx = 0;
}

act_col::act_col(unsigned int pos, unsigned int net, unsigned int idx)
{
	this->pos = pos;
	this->net = net;
	this->idx = idx;
}

act_col::~act_col()
{
}


act_ovr::act_ovr()
{
	ports = 0;
	idx = -1;
}

act_ovr::~act_ovr()
{
}

act_stack::act_stack()
{
	A_INIT(mos);
	A_INIT(sel);
	A_INIT(col);
	A_INIT(ovr);
	stage[0] = 0;
	stage[1] = 0;
}

act_stack::~act_stack()
{
	A_FREE(mos);
	A_FREE(sel);
	A_FREE(col);
	A_FREE(ovr);
}

void act_stack::init(unsigned int nets)
{
	A_NEWP(ovr, act_ovr_t, nets);
	for (int i = 0; i < nets; i++) {
		A_NEXT(ovr) = act_ovr();
		A_INC(ovr);
	}
	layer.init(nets);
}

void act_stack::stash()
{
	A_DELETE(col, stage[0], stage[1] - stage[0]);
	stage[1] = A_LEN(col);
	layer.stash();
}

void act_stack::commit()
{
	A_DELETE(col, stage[1], A_LEN(col) - stage[1]);
	stage[0] = A_LEN(col);
	stage[1] = A_LEN(col);
	layer.commit();
}

void act_stack::clear()
{
	A_LEN(col) = stage[1];
	layer.clear();
}

void act_stack::reset()
{
	A_LEN(col) = stage[0];
	stage[1] = stage[0];
	layer.reset();
}

void act_stack::print(const char *dev)
{
	for (int i = 0; i < A_LEN(sel); i++) {
		printf("%s b:%d idx:%d flip:%d\n", dev, mos[sel[i].idx].bulk, sel[i].idx, sel[i].flip);
		if (not sel[i].flip) {
			printf("  s:%d\n", mos[sel[i].idx].source);
			for (int j = 0; j < A_LEN(mos[sel[i].idx].gate); j++)
				printf("  g:%d w:%d l:%d\n",
				  mos[sel[i].idx].gate[j].net,
				  mos[sel[i].idx].gate[j].width,
				  mos[sel[i].idx].gate[j].length);
			printf("  d:%d\n", mos[sel[i].idx].drain);
		} else {
			printf("  d:%d\n", mos[sel[i].idx].drain);
			for (int j = A_LEN(mos[sel[i].idx].gate)-1; j >= 0; j--)
				printf("  g:%d w:%d l:%d\n",
				  mos[sel[i].idx].gate[j].net,
				  mos[sel[i].idx].gate[j].width,
				  mos[sel[i].idx].gate[j].length);
			printf("  s:%d\n", mos[sel[i].idx].source);
		}
	}
}

void act_stack::count_ports()
{
	for (int i = 0; i < A_LEN(mos); i++) {
		ovr[mos[i].source].ports += 1;
		ovr[mos[i].drain].ports += 1;
		for (int j = 0; j < A_LEN(mos[i].gate); j++) {
			ovr[mos[i].gate[j].net].ports += 1;
		}
	}
}

void act_stack::collect(layout_task *task)
{
	for (int i = 0; i < A_LEN(mos); i++) {
		for (int j = A_LEN(mos)-1; j > i; j--) {
			if (mos[i].drain == mos[j].source and task->nets[mos[i].drain].ports == 2) {
				A_NEWP(mos[i].gate, act_gate_t, A_LEN(mos[j].gate));
				for (int k = 0; k < A_LEN(mos[j].gate); k++) {
					A_NEXT(mos[i].gate) = mos[j].gate[k];
					A_INC(mos[i].gate);
				}
				mos[i].drain = mos[j].drain;
				A_DELETE(mos,j,1);
			} else if (mos[i].source == mos[j].drain and task->nets[mos[i].source].ports == 2) {
				A_NEWP(mos[j].gate, act_gate_t, A_LEN(mos[i].gate));
				for (int k = 0; k < A_LEN(mos[i].gate); k++) {
					A_NEXT(mos[j].gate) = mos[i].gate[k];
					A_INC(mos[j].gate);
				}
				A_ASSIGN(mos[i].gate, mos[j].gate);
				mos[i].source = mos[j].source;
				A_DELETE(mos,j,1);
			}
		}
	}
}

void act_stack::stage_mos(int net)
{
	if (ovr[net].idx >= 0) {
		for (int r = A_LEN(col)-1; r >= 0 and col[r].net != net; r--) {
			// I should only push this edge if this signal needs to be routed over
			// the cell, and the edge is not already in the graph.
			// TODO: I need to account for merged source/drain
			if (ovr[col[r].net].ports > 1
				and not layer.has_edge(col[r].net, net)) {
				layer.push_edge(col[r].net, net);
			}
		}
	}

	A_NEW(col, act_col_t);
	new (&A_NEXT(col)) act_col_t(0, net, ovr[net].idx+1);
	A_INC(col);
}

void act_stack::stage_stack(int sel, int flip)
{
	if (not flip) {
		if (A_LEN(col) > 0 and mos[sel].source != col[A_LEN(col)-1].net) {
			stage_mos(mos[sel].source);
		} else {
			// TODO: I have to decrement this if it is unstaged
			ovr[mos[sel].source].idx += 1;
		}
		for (int g = 0; g < A_LEN(mos[sel].gate); g++) {
			stage_mos(mos[sel].gate[g].net);
		}
		stage_mos(mos[sel].drain);
	} else {
		if (A_LEN(col) > 0 and mos[sel].drain != col[A_LEN(col)-1].net) {
			stage_mos(mos[sel].drain);
		} else {
			// TODO: I have to decrement this if it is unstaged
			ovr[mos[sel].drain].idx += 1;
		}
		for (int g = A_LEN(mos[sel].gate)-1; g >= 0; g--) {
			stage_mos(mos[sel].gate[g].net);
		}
		stage_mos(mos[sel].source);
	}
}

layout_task::layout_task()
{
	A_INIT(nets);
}

layout_task::~layout_task()
{
	A_FREE(nets);
}

void ActLayoutPass::collect_stacks(layout_task *task)
{
	for (int m = 0; m < 2; m++) {
		task->stack[m].count_ports();
	}

	for (int i = 0; i < A_LEN(task->nets); i++) {
		for (int m = 0; m < 2; m++) {
			task->nets[i].ports += task->stack[m].ovr[i].ports;
		}
	}

	for (int m = 0; m < 2; m++) {
		task->stack[m].collect(task);
	}
}

void compute_stack_order(layout_task *task)
{
	int j[2] = {0,0};
	while (j[0] < A_LEN(task->stack[0].mos) or j[1] < A_LEN(task->stack[1].mos)) {
		// Alternate picking PMOS/NMOS stacks
		// Pick the stack that minimizes:
		//   - The number of color assignments in the over-the-cell routing problems
		//   - The number of edges introduced into the over-the-cell routing problems
		//   - The total expected horizontal distance between nets connected between the nmos and pmos stacks
		
		// Pick whichever stack currently has fewer columns as long as there are transistors left to route in that stack
		int i = 0;
		if (j[0] >= A_LEN(task->stack[0].mos) or (j[1] < A_LEN(task->stack[1].mos) and A_LEN(task->stack[1].col) < A_LEN(task->stack[0].col))) {
			i = 1;
		}

		unsigned int sel;
		unsigned int flip;
		unsigned int cost = 0;

		for (int k = 0; k < A_LEN(task->stack[i].mos); k++) {
			if (not task->stack[i].mos[k].selected) {
				for (int f = 0; f < 2; f++) {
					unsigned int next = 0;
					
					// compute the cost of selecting this stack
					task->stack[i].stage_stack(k, f);
					
					next += task->stack[i].layer.stash_cost();

					if (cost == 0 or next < cost) {
						sel = k;
						flip = f;
						cost = next;
						task->stack[i].stash();
					} else {
						task->stack[i].clear();
					}
				}
			}
		}

		task->stack[i].commit();
		A_NEW(task->stack[i].sel, act_dev_sel_t);
		A_NEXT(task->stack[i].sel) = act_dev_sel_t(sel, flip);
		A_INC(task->stack[i].sel); 
		task->stack[i].mos[sel].selected = 1;
		j[i] += 1;
	}
}

void compute_channel_routes(layout_task *task)
{
}

void ActLayoutPass::process_cell(Process *p)
{
	if (!p->isExpanded()) {
		fatal_error ("Process has not been expanded.");
	}

	netlist_t *n = np->getNL(p);
	if (n == NULL) {
		fatal_error ("Process has not been netlisted.");
	}
	
	layout_task task;

	int max_net_id = -1;
	for (node_t *x = n->hd; x; x = x->next) {
		if (x->i > max_net_id) {
			max_net_id = x->i;
		}
	}
	max_net_id += 1;

	A_NEWP(task.nets,net_t,max_net_id);
	for (int i = 0; i < max_net_id; i++) {
		A_NEXT(task.nets) = net_t();
		A_INC(task.nets);
	}
	task.stack[0].init(max_net_id);
	task.stack[1].init(max_net_id);

	printf("max_net_id: %d\n", max_net_id);

	for (node_t *x = n->hd; x; x = x->next) {
		task.nets[x->i].node = x;

		listitem_t *li;

		for (li = list_first(x->e); li; li = list_next(li)) {
			edge_t *e = (edge_t *)list_value(li);
			
			int len_repeat, width_repeat;
      int width_last;
      int il, iw;
      int w, l;
      int fold;

			
			if (e->visited || e->pruned)
				continue;
			
			e->visited = 1;
	
			w = e->w;
			l = e->l;

			/* discretize lengths */
			len_repeat = e->nlen;
			if (discrete_len > 0) {
				l = discrete_len;
			}

			if (e->type == EDGE_NFET) {
				fold = n_fold;
			} else {
				Assert (e->type == EDGE_PFET, "Hmm");
				fold = p_fold;
			}

			width_repeat = e->nfolds;

			for (int il = 0; il < len_repeat; il++) {
				for (int iw = 0; iw < width_repeat; iw++) {
					if (width_repeat > 1) {
						w = EDGE_WIDTH (e, iw);
					} else {
						w = e->w;
					}

					A_NEW(task.stack[e->type].mos, act_dev_t);
					new (&A_NEXT(task.stack[e->type].mos)) act_dev_t(e->g->i, e->a->i, e->b->i, e->bulk->i, w, l);
					A_INC(task.stack[e->type].mos);
				}
			}
		}
	}

	for (node_t *x = n->hd; x; x = x->next) {
    listitem_t *li;
    for (li = list_first (x->e); li; li = list_next (li)) {
      edge_t *e = (edge_t *)list_value (li);
      e->visited = 0;
    }
  }

	// TODO: figure out the stack ordering
	// TODO: route above and below the stacks
	// TODO: route the channel
	// TODO: parse the DRC rule files
	// TODO: add the DRC constraints into the routers

	
	collect_stacks(&task);
	compute_stack_order(&task);

	char buf[1000];
	for (int i = 0; i < A_LEN(task.nets); i++) {
		if (task.nets[i].node != NULL and task.nets[i].node->v != NULL) {
			ActId *id = task.nets[i].node->v->v->id->toid();
			id->sPrint(buf, 1000);
			printf("%d: \"%s\" pmos:%d nmos:%d\n", i, buf, task.stack[1].layer.color[i], task.stack[0].layer.color[i]);
		} else {
			printf("%d: \"\" pmos:%d nmos:%d\n", i, task.stack[1].layer.color[i], task.stack[0].layer.color[i]);
		}
	}

	task.stack[0].print("nmos");
	task.stack[1].print("pmos");
}

void *ActLayoutPass::local_op (Process *p, int mode)
{
	if (p and p->isCell()) {
		process_cell(p);
	}
	return NULL;
}

void ActLayoutPass::free_local (void *v)
{
}

ActLayoutPass::ActLayoutPass (Act *a) : ActPass (a, "cells2layout")
{
	np = (ActNetlistPass*)a->pass_find("prs2net");
  if (!np) {
    np = new ActNetlistPass (a);
  }
	Assert(np, "What?");
  AddDependency ("prs2net");

  n_fold = config_get_int ("net.fold_nfet_width");
  p_fold = config_get_int ("net.fold_pfet_width");
  discrete_len = config_get_int ("net.discrete_length");

  lambda = config_get_real ("net.lambda");
}

ActLayoutPass::~ActLayoutPass ()
{
}
