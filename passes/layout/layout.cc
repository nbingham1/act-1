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
	A_INIT(act_coords);
}

net::~net()
{
	A_FREE(act_coords);
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
}

act_dev::~act_dev()
{
	A_FREE(this->gate);
}

layout_task::layout_task()
{
	A_INIT(pmos);
	A_INIT(nmos);
	A_INIT(nets);
}

layout_task::~layout_task()
{
	A_FREE(pmos);
	A_FREE(nmos);
	A_FREE(nets);
}

void ActLayoutPass::collect_stacks(layout_task *task)
{
	A_DECL(unsigned int, count);
	A_NEWP(count, unsigned int, A_LEN(task->nets));
	
	for (int i = 0; i < A_LEN(task->nets); i++) {
		A_NEXT(count) = 0;
		A_INC(count);
	}

	for (int i = 0; i < A_LEN(task->pmos); i++) {
		count[task->pmos[i].source] += 1;
		count[task->pmos[i].drain] += 1;
	}

	for (int i = 0; i < A_LEN(task->nmos); i++) {
		printf("%d,%d/%d\n", task->nmos[i].source, task->nmos[i].drain, A_LEN(count));
		count[task->nmos[i].source] += 1;
		count[task->nmos[i].drain] += 1;
	}

	for (int i = 0; i < A_LEN(task->pmos); i++) {
		for (int j = A_LEN(task->pmos)-1; j > i; j--) {
			if (task->pmos[i].drain == task->pmos[j].source and count[task->pmos[i].drain] == 2) {
				A_NEWP(task->pmos[i].gate, act_gate_t, A_LEN(task->pmos[j].gate));
				for (int k = 0; k < A_LEN(task->pmos[j].gate); k++) {
					A_NEXT(task->pmos[i].gate) = task->pmos[j].gate[k];
					A_INC(task->pmos[i].gate);
				}
				task->pmos[i].drain = task->pmos[j].drain;
				A_DELETE(task->pmos,j);
			} else if (task->pmos[i].source == task->pmos[j].drain and count[task->pmos[i].source] == 2) {
				A_NEWP(task->pmos[j].gate, act_gate_t, A_LEN(task->pmos[i].gate));
				for (int k = 0; k < A_LEN(task->pmos[i].gate); k++) {
					A_NEXT(task->pmos[j].gate) = task->pmos[i].gate[k];
					A_INC(task->pmos[j].gate);
				}
				A_ASSIGN(task->pmos[i].gate, task->pmos[j].gate);
				task->pmos[i].source = task->pmos[j].source;
				A_DELETE(task->pmos,j);
			}
		}
	}

	for (int i = 0; i < A_LEN(task->nmos); i++) {
		for (int j = A_LEN(task->nmos)-1; j > i; j--) {
			if (task->nmos[i].drain == task->nmos[j].source and count[task->nmos[i].drain] == 2) {
				A_NEWP(task->nmos[i].gate, act_gate_t, A_LEN(task->nmos[j].gate));
				for (int k = 0; k < A_LEN(task->nmos[j].gate); k++) {
					A_NEXT(task->nmos[i].gate) = task->nmos[j].gate[k];
					A_INC(task->nmos[i].gate);
				}
				task->nmos[i].drain = task->nmos[j].drain;
				A_DELETE(task->nmos,j);
			} else if (task->nmos[i].source == task->nmos[j].drain and count[task->nmos[i].source] == 2) {
				A_NEWP(task->nmos[j].gate, act_gate_t, A_LEN(task->nmos[i].gate));
				for (int k = 0; k < A_LEN(task->nmos[i].gate); k++) {
					A_NEXT(task->nmos[j].gate) = task->nmos[i].gate[k];
					A_INC(task->nmos[j].gate);
				}
				A_ASSIGN(task->nmos[i].gate, task->nmos[j].gate);
				task->nmos[i].source = task->nmos[j].source;
				A_DELETE(task->nmos,j);
			}
		}
	}
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

	printf("max_net_id: %d\n", max_net_id);

	for (node_t *x = n->hd; x; x = x->next) {
		printf("node: %d\n", x->i);
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

					if (e->type == 0) {
						A_NEW(task.nmos, act_dev_t);
						new (&A_NEXT(task.nmos)) act_dev_t(e->g->i, e->a->i, e->b->i, e->bulk->i, w, l);
						A_INC(task.nmos);
					} else {
						A_NEW(task.pmos, act_dev_t);
						new (&A_NEXT(task.pmos)) act_dev_t(e->g->i, e->a->i, e->b->i, e->bulk->i, w, l);
						A_INC(task.pmos);
					}
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

	char buf[1000];
	for (int i = 0; i < A_LEN(task.nets); i++) {
		if (task.nets[i].node != NULL and task.nets[i].node->v != NULL) {
			ActId *id = task.nets[i].node->v->v->id->toid();
			id->sPrint(buf, 1000);
			printf("%d: \"%s\"\n", i, buf);
		} else {
			printf("%d: \"\"\n", i);
		}
	}

	for (int i = 0; i < A_LEN(task.pmos); i++) {
		printf("pmos b:%d s:%d\n", task.pmos[i].bulk, task.pmos[i].source);
		for (int j = 0; j < A_LEN(task.pmos[i].gate); j++)
			printf("  g:%d w:%d l:%d\n", task.pmos[i].gate[j].net, task.pmos[i].gate[j].width, task.pmos[i].gate[j].length);
		printf("  d:%d\n", task.pmos[i].drain);
	}

	for (int i = 0; i < A_LEN(task.nmos); i++) {
		printf("nmos b:%d s:%d\n", task.nmos[i].bulk, task.nmos[i].source);
		for (int j = 0; j < A_LEN(task.nmos[i].gate); j++)
			printf("  g:%d w:%d l:%d\n", task.nmos[i].gate[j].net, task.nmos[i].gate[j].width, task.nmos[i].gate[j].length);
		printf("  d:%d\n", task.nmos[i].drain);
	}
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
