/*******************************************************************************
  Copyright (c) 2011-2014 Dmitry Matveev <me@dmitrymatveev.co.uk>
  Copyright (c) 2014-2016 Vladimir Kondratiev <wulf@cicgroup.ru>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*******************************************************************************/

#ifndef __DEP_LIST_H__
#define __DEP_LIST_H__

#include "compat.h"

#include <sys/types.h> /* ino_t */
#include <sys/stat.h>  /* mode_t */

#define S_IFUNK 0000000 /* mode_t extension. File type is unknown */
#define S_ISUNK(m) (((m) & S_IFMT) == S_IFUNK)

typedef struct dep_item {
    ino_t inode;
    mode_t type;
    char path[];
} dep_item;

typedef struct dep_node {
    SLIST_ENTRY(dep_node) next;
    dep_item *item;
} dep_node;

typedef struct dep_list {
    SLIST_HEAD(, dep_node) head;
} dep_list;

typedef void (* no_entry_cb)     (void *udata);
typedef void (* single_entry_cb) (void *udata, dep_item *di);
typedef void (* dual_entry_cb)   (void *udata,
                                  dep_item *from_di,
                                  dep_item *to_di);
typedef void (* list_cb)         (void *udata, const dep_list *list);


typedef struct traverse_cbs {
    dual_entry_cb    unchanged;
    single_entry_cb  added;
    single_entry_cb  removed;
    single_entry_cb  replaced;
    dual_entry_cb    overwritten;
    dual_entry_cb    moved;
    list_cb          many_added;
    list_cb          many_removed;
    no_entry_cb      names_updated;
} traverse_cbs;

dep_item* di_create       (const char *path, ino_t inode, mode_t type);
void      di_free         (dep_item *di);

dep_list* dl_create       ();
dep_node* dl_insert       (dep_list *dl, dep_item *di);
void      dl_remove_after (dep_list *dl, dep_node *dn);
void      dl_print        (const dep_list *dl);
dep_list* dl_shallow_copy (const dep_list *dl);
void      dl_shallow_free (dep_list *dl);
void      dl_free         (dep_list *dl);
dep_list* dl_listing      (int fd);

int
dl_calculate (dep_list            *before,
              dep_list            *after,
              const traverse_cbs  *cbs,
              void                *udata);


#endif /* __DEP_LIST_H__ */
