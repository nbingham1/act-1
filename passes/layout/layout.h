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

typedef struct act_dev_group
{
	act_dev_group();
	act_dev_group(unsigned int source, unsigned int drain);
	~act_dev_group();

	unsigned int source;
	unsigned int drain;
} act_dev_group_t;

enum act_dev_type {
	ACT_NMOS = 0,
	ACT_PMOS = 1
};

typedef struct act_dev
{
	act_dev();
	act_dev(unsigned int type, unsigned int gate, act_dev_group_t group, act_size_spec_t *size);
	~act_dev();

	unsigned char type;
	unsigned int gate;
	unsigned int source;
	unsigned int drain;
	act_size_spec_t *size;
} act_dev_t;

typedef struct act_cell
{
	act_cell();
	~act_cell();

	Process *p;
	A_DECL(ActId*, nets);
	A_DECL(act_dev_t, devs);
	
	unsigned int netId(ActId *name);
} act_cell_t;

class ActLayoutPass : public ActPass {
public:
  ActLayoutPass (Act *a);
  ~ActLayoutPass ();

  int run (Process *p = NULL);

  void Print (FILE *fp);

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

	void extract_devices(act_cell_t *c, act_prs_expr_t *e, act_size_spec_t **sz, act_dev_group_t group, unsigned char type = ACT_NMOS);
	void extract_stack(act_prs_lang_t *lang, act_cell_t *c);
	act_cell_t *extract_cell(Process *p);
	void collect_cells(Process *p);
};


#endif /* __ACT_PASS_LAYOUT_H__ */
