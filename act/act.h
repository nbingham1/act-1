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
#ifndef __ACT_H__
#define __ACT_H__

#include <act/types.h>
#include <act/lang.h>
#include <act/body.h>
#include <act/value.h>
#include <map>
#include <unordered_set>

/**
 * @file act.h
 *       Contains top-level initialization/management functions for
 *       the ACT library
 */

class ActPass;
class Log;

/**
 *   The main Act class used to read in an ACT file and create basic
 *   data structures
 */
class Act {
 public:
  /**
   * Initialize the ACT library
   *
   *  @param argc is a pointer to argc (command-line processing)
   *  @param argv is a pointer to argv (command-line processing)
   *
   *  @return *argc and *argv are modified to reflect the command-line
   *  options left after ACT has extracted the ones it understands
   */
  static void Init (int *argc, char ***argv);


  /**
   * Maximum depth of recursion for expanding types
   */
  static int max_recurse_depth;

  /**
   * Maximum number of iterations for general loops
   */
  static int max_loop_iterations;

  /**
   * Warn on empty selection in main ACT language
   */
  static int warn_emptyselect;

  /**
   * Warn if Expand is called on something that is already expanded
   */
  static int warn_double_expand;

  /**
   * Parser flags 
   */
  static int emit_depend;

  /**
   * Config debugging
   */
  static void config_info (const char *s);


  /**
   * Create an act data structure for the specified input file
   *
   * @param s is the name of the file containing the top-level ACT. If
   * NULL, then the library is initialized without any ACT file being
   * read.
   */
  Act (const char *s = NULL);
  ~Act ();

  /** 
   * Merge in ACT file "s" into current ACT database
   *
   * @param s is the name of an ACT file
   */
  void Merge (const char *s);


  /**
   * Expand types
   */
  void Expand ();


  /**
   * Install string mangling functionality
   *
   * @param s is a string corresponding to the list of characters to
   * be mangled.
   */
  void mangle (char *s);

  int mangle_active() { return any_mangling; } /**< @return 1 if
						  mangling is active, 
						  0 otherwise */

  /** 
   * mangle string from src to dst.
   * @param src is the source string
   * @param dst is the destination string
   * @param sz is the space available in the destination string
   * @return 0 on success, -1 on error
   */  
  int mangle_string (const char *src, char *dst, int sz);
  
  /** 
   * unmangle string from src to dst.
   * @param src is the source string
   * @param dst is the destination string
   * @param sz is the space available in the destination string
   * @return 0 on success, -1 on error
   */  
  int unmangle_string (const char *src, char *dst, int sz);

  /**
   * Mangle fprintf functionality
   */
  void mfprintf (FILE *fp, const char *s, ...);

  /**
   * Unmangle fprintf functionality
   */
  void ufprintf (FILE *fp, const char *s, ...);

  /**
   * Mangle snprintf functionality
   */
  int msnprintf (char *fp, int sz, const char *s, ...);

  /**
   * Unmangle snprintf functionality
   */
  int usnprintf (char *fp, int sz, const char *s, ...);

  /**
   * Non-standard mangling for user-defined types.
   * @param omit_ns is 1 if you don't want to include the namespace in
   * the string
   */
  void msnprintfproc (char *fp, int sz, UserDef *, int omit_ns = 0);

  /**
   * Non-standard mangling for user-defined types
   * @param omit_ns is 1 if you don't want to include the namespace in
   * the string
   */
  void mfprintfproc (FILE *fp, UserDef *, int omit_ns = 0);

  /* 
     API functions
  */

  /**
   * Find a process given a name
   * @param s is the name of the process
   * @return process pointer, or NULL if not found
   */
  Process *findProcess (const char *s);

  /**
   * Find a process within a namespace
   * @param s is the name of the process
   * @param ns is the ACT namespace
   * @return the process pointer if found, NULL otherwise
   */
  Process *findProcess (ActNamespace *ns, const char *s);

  /**
   * Find a user-defined type
   * @param s is the name of the type
   * @return the UserDef pointer if found, NULL otherwise
   */
  UserDef *findUserdef (const char *s);

  ActNamespace *findNamespace (const char *s);
  ActNamespace *findNamespace (ActNamespace *, const char *);
  ActNamespace *Global() { return gns; }

  /*
    Dump to a file 
  */
  void Print (FILE *fp);

  void pass_register (const char *name, ActPass *p);
  ActPass *pass_find (const char *name);
  void pass_unregister (const char *name);
  const char *pass_name (const char *name);

private:
  TypeFactory *tf;		/* type factory for the file */
  ActNamespace *gns;		/* global namespace */

  char mangle_characters[256];
  int inv_map[256];
  int any_mangling;
  int mangle_langle_idx;  /* index of '<' */
  int mangle_min_idx;     /* index of the min of , . { } */
  int mangle_mode;

  static Log *L;

  struct Hashtable *passes;	// any ActPass-es
};

class ActPass {
protected:
  int _finished;		// has the pass finished execution?
  Act *a;			// main act data structure
  list_t *deps;			// ActPass dependencies
  const char *name;

  
public:
  ActPass (Act *_a, const char *name); // create, initialize, and
				       // register pass
  
  ~ActPass ();			       // release storage


  int AddDependency (const char *pass); // insert dependency on an
					// actpass

  int rundeps (Process *p = NULL);

  const char *getName();
  
  virtual int run(Process *p = NULL); // run pass on a process; NULL =
				      // top level
  
  int completed()  { return (_finished == 2) ? 1 : 0; }
  int pending()  { return (_finished == 1) ? 1 : 0; }
  void *getMap (Process *p);
  virtual void run_recursive (Process *p = NULL, int mode = 0);
  
private:
  virtual void *local_op (Process *p, int mode = 0);
  virtual void free_local (void *);

  int init (); // initialize or re-initialize
  void recursive_op (Process *p, int mode = 0);
  void init_map ();
  void free_map ();
  std::map<Process *, void *> *pmap;
  std::unordered_set<Process *> *visited_flag;

public:
  
};


/* this should be elsewhere */
Expr *const_expr (int);

#endif /* __ACT_H__ */
