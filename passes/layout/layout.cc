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

act_dev::act_dev(unsigned int gate, unsigned int source, unsigned int drain, unsigned int bulk, unsigned int width, unsigned int length)
{
	this->gate = gate;
	this->source = source;
	this->drain = drain;
	this->bulk = bulk;
	this->width = width;
	this->length = length;
}

act_dev::~act_dev()
{
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

void ActLayoutPass::process_cell(Process *p)
{
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
						A_NEXT(task.nmos) = act_dev_t(e->g->i, e->a->i, e->b->i, e->bulk->i, w, l);
						A_INC(task.nmos);
					} else {
						A_NEW(task.pmos, act_dev_t);
						A_NEXT(task.pmos) = act_dev_t(e->g->i, e->a->i, e->b->i, e->bulk->i, w, l);
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
		printf("pmos g:%d s:%d d:%d b:%d w:%d l:%d\n", task.pmos[i].gate, task.pmos[i].source, task.pmos[i].drain, task.pmos[i].bulk, task.pmos[i].width, task.pmos[i].length);
	}

	for (int i = 0; i < A_LEN(task.nmos); i++) {
		printf("nmos g:%d s:%d d:%d b:%d w:%d l:%d\n", task.nmos[i].gate, task.nmos[i].source, task.nmos[i].drain, task.nmos[i].bulk, task.nmos[i].width, task.nmos[i].length);
	}

}

void *ActLayoutPass::local_op (Process *p, int mode)
{
	if (p and p->isCell()) {
		if (p->isExpanded()) {
			process_cell(p);
		} else {
			fatal_error ("Process has not been expanded.");
		}
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
