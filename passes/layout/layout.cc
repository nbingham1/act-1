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

act_dev_group::act_dev_group()
{
}

act_dev_group::act_dev_group(unsigned int source, unsigned int drain)
{
	this->source = source;
	this->drain = drain;
}

act_dev_group::~act_dev_group()
{
} 

act_dev::act_dev()
{
}

act_dev::act_dev(unsigned int type, unsigned int gate, act_dev_group_t group, act_size_spec_t *size)
{
	this->type = type;
	this->gate = gate;
	this->source = group.source;
	this->drain = group.drain;
	this->size = size;
}

act_dev::~act_dev()
{
}

act_cell::act_cell()
{
	A_INIT(nets);
	A_INIT(devs);
}

act_cell::~act_cell()
{
}

unsigned int act_cell::netId(ActId *name)
{
	for (unsigned int i = 0; i < A_LEN(nets); i++)
		if (nets[i] == name or (nets[i] != NULL and name->isEqual(nets[i])))
			return i;
	return A_LEN(nets);
}

void ActLayoutPass::extract_devices(act_cell_t *c, act_prs_expr_t *e, act_size_spec_t **sz, act_dev_group_t group, unsigned char type)
{
	if (e)
	{
		unsigned int node;
		switch (e->type) {
		case ACT_PRS_EXPR_AND:
			A_NEW(c->nets, ActId*);
			A_NEXT(c->nets) = NULL;
			node = A_LEN(c->nets);
			A_INC(c->nets);
			extract_devices(c, e->u.e.l, sz, act_dev_group_t(group.source, node), type);
			extract_devices(c, e->u.e.r, sz, act_dev_group(node, group.drain), type);
			break;

		case ACT_PRS_EXPR_OR:
			extract_devices(c, e->u.e.l, sz, group, type);
			extract_devices(c, e->u.e.r, sz, group, type);
			break;

		case ACT_PRS_EXPR_NOT:
			extract_devices(c, e->u.e.l, sz, group, 1-type);
			break;

		case ACT_PRS_EXPR_LABEL:
			break;

		case ACT_PRS_EXPR_VAR:
    if (e->u.v.sz) {
      *sz = e->u.v.sz;
    } else {
      e->u.v.sz = *sz;
    }
		node = c->netId(e->u.v.id);
		if (node >= A_LEN(c->nets)) {
			A_NEW(c->nets, ActId*);
			A_NEXT(c->nets) = e->u.v.id;
			A_INC(c->nets);
		}

		A_NEW(c->devs, act_dev_t);
		A_NEXT(c->devs) = act_dev_t(type, node, group, e->u.v.sz);
		A_INC(c->devs);
    break;
    
		case ACT_PRS_EXPR_ANDLOOP:
		case ACT_PRS_EXPR_ORLOOP:
			Assert (0, "Loops?!");
			break;

		case ACT_PRS_EXPR_TRUE:
		case ACT_PRS_EXPR_FALSE:
			break;
		}
	}
}

void ActLayoutPass::extract_stack(act_prs_lang_t *lang, act_cell_t *c)
{
	if (lang->type == ACT_PRS_RULE)
	{
		act_size_spec_t *sz = NULL;
		act_dev_group group(lang->u.one.dir, c->netId(lang->u.one.id));
		if (group.drain >= A_LEN(c->nets)) {
			A_NEW(c->nets, ActId*);
			A_NEXT(c->nets) = lang->u.one.id;
			A_INC(c->nets);
		}

		extract_devices(c, lang->u.one.e, &sz, group);
	}
	else
	{
		fatal_error ("Cell `cell::%s': no fets/subckts/dup trees allowed", c->p->getName());
	}
}

act_cell_t *ActLayoutPass::extract_cell(Process *p)
{
	/* cells in the "cells" namespace are very special.

		 1. no aliases internally
		 2. defcell has two ports: in[n], out[m]
		 3. only prs for out[m]'s are present

		 4. cell names are g<#> (for "gate")
		 5. pass gate cells are t0,t1,n0,n1,p0,p1
	*/
	if (p->getName()[0] == 'g') {
		/* ok, gate! */
		if (p->getNumParams() != 0) {
			fatal_error ("Unexpected cell `%s' in cell namespace (has parameters?)", p->getName());
		}
		if (p->getNumPorts() != 2) {
			fatal_error ("Cell `cell::%s': More than two ports");
		}
		if ((strcmp (p->getPortName (0), "in") != 0) ||
		    (strcmp (p->getPortName (1), "out") != 0)) {
			fatal_error ("Cell `cell::%s': Ports should be in/out", p->getName());
		}
		InstType *in_t, *out_t;
		in_t = p->getPortType (0);
		out_t = p->getPortType (1);
		
		/* in_t must be a bool array or bool
 out_t must be a bool array or bool
		*/
		if (!TypeFactory::isBoolType (in_t) || !TypeFactory::isBoolType (out_t)) {
			fatal_error ("Cell `cell::%s': Port base types must `bool'", p->getName());
		}

		/* sanity check prs */
		act_prs *prs = p->getprs();
		if (prs && prs->next) {
			fatal_error ("Cell `cell::%s': More than one prs body", p->getName());
		}

		act_prs_lang_t *l;
		Expr *treeval;
		l = NULL;
		treeval = NULL;
		act_cell_t *c = NULL;
		if (prs != NULL) {
			c = new act_cell_t();
			c->p = p;

			A_NEW(c->nets, ActId*);
			A_NEXT(c->nets) = prs->gnd;
			A_INC(c->nets);
			A_NEW(c->nets, ActId*);
			A_NEXT(c->nets) = prs->vdd;
			A_INC(c->nets);
			
			l = prs->p;
			if (l && l->type == ACT_PRS_TREE) {
				if (l->next) {
					fatal_error ("Cell `cell::%s': only one tree permitted", p->getName());
				}
				l = l->u.l.p;
			}
			while (l) {
				Assert (l->type != ACT_PRS_LOOP, "Expanded?!");
				if (l->type == ACT_PRS_GATE || l->type == ACT_PRS_SUBCKT ||
						l->type == ACT_PRS_TREE) {
					fatal_error ("Cell `cell::%s': no fets/subckts/dup trees allowed", p->getName());
				}
				else if (l->type == ACT_PRS_RULE) {
					extract_stack(l, c);
				}
				l = l->next;
			}
			l = prs->p;
			if (l && l->type == ACT_PRS_TREE) {
				treeval = l->u.l.lo;
				l = l->u.l.p;
				extract_stack(l, c);
			}

			printf ("CELL: %s\n", p->getName());
			for (int i = 0; i < A_LEN(c->devs); i++) {
				printf("%s g:%d s:%d d:%d", c->devs[i].type == ACT_NMOS ? "nmos" : "pmos", c->devs[i].gate, c->devs[i].source, c->devs[i].drain);
				if (c->devs[i].size == NULL) {
					printf(" w,l:null");
				} else {
					if (c->devs[i].size->w == NULL or c->devs[i].size->w->type != E_INT) {
						printf(" w:<e>");
					} else {
						printf(" w:%u", c->devs[i].size->w->u.v);
					}
					
					if (c->devs[i].size->l == NULL or c->devs[i].size->l->type != E_INT) {
						printf(" l:<e>");
					} else {
						printf(" l:%u", c->devs[i].size->l->u.v);
					}
				}
				printf("\n");

			}
		}

		return c;
	}
	
	return NULL;
	// cells are small, so we can just brute-force the transistor stack ordering. Then we can use Channel Routing to route all of the nets.
}

void ActLayoutPass::collect_cells(Process *p)
{
  ActNamespace *cell_ns = a->findNamespace ("cell");
	if (!cell_ns) {
		return;
	}

	ActTypeiter it(cell_ns);

	A_DECL (const char *, xcell);
  A_INIT (xcell);

	for (it = it.begin(); it != it.end(); it++) {
    Type *u = (*it);
    Process *p = dynamic_cast<Process *>(u);
    if (!p) continue;

    if (!p->isCell()) {
      continue;
    }

		if ((!p->isExpanded()) && (p->getNumParams() == 0)) {
      A_NEW (xcell, const char *);
      A_NEXT (xcell) = p->getName();
      A_INC (xcell);
    }
	}

	for (int i=0; i < A_LEN (xcell); i++) {
    char buf[10240];
    sprintf (buf, "%s<>", xcell[i]);
    Type *u = cell_ns->findType (xcell[i]);
    Assert (u, "What?");
    Process *p = dynamic_cast<Process *>(u);
    Assert (p, "What?");
    if (!cell_ns->findType (buf)) {
      /* create expanded version */
      p->Expand (cell_ns, cell_ns->CurScope(), 0, NULL);
    }
  }
  A_FREE (xcell);

	for (it = it.begin(); it != it.end(); it++) {
    Type *u = *it;
    Process *p = dynamic_cast <Process *>(u);
    if (!p) {
      continue;
    }
    if (!p->isCell()) {
      continue;
    }
    if (!p->isExpanded()) {
      continue;
    }

		act_cell_t *c = extract_cell(p);
		if (c != NULL)
			delete c;
  }
}

void *ActLayoutPass::local_op (Process *p, int mode)
{
	collect_cells(p);
	return NULL;
}

void ActLayoutPass::free_local (void *v)
{
}

int ActLayoutPass::run (Process *p)
{
  int ret = ActPass::run (p);

  return ret;
}


void ActLayoutPass::Print (FILE *fp)
{
}


ActLayoutPass::ActLayoutPass (Act *a) : ActPass (a, "cells2layout")
{
}

ActLayoutPass::~ActLayoutPass ()
{
}
