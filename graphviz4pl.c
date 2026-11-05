/*  Part of SWI-Prolog

    Author:        Jan Wielemaker
    E-mail:        jan@swi-prolog.org
    WWW:           http://www.swi-prolog.org
    Copyright (c)  2026, SWI-Prolog Solutions b.v.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/


#ifdef __linux__
#define _GNU_SOURCE			/* dladdr() */
#endif
#include <config.h>
#include <SWI-Prolog.h>
#include <SWI-Stream.h>
#include <gvc.h>
#include <gvplugin.h>
#include <types.h>
#include <pthread.h>
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#include <string.h>
#include <limits.h>
#include <stdlib.h>

/* Lay out a graph using the graphviz libraries.

Only the layout part of graphviz is used: no renderer and no text
layout plugin is loaded.  Graphviz estimates the size of text from
built-in font metrics, which does not matter if the caller gives the
nodes and labels a size without text, as library(graphviz_layout) of
xpce does.

The layout plugins are registered by this library rather than by the
plugin loading of graphviz, which also loads the text layout plugin
(pango, cairo, fontconfig, ...) even if there is no text.  The dot
layout plugin is linked if possible.  The other layout plugins, notably
the neato plugin that provides neato, fdp, sfdp, circo, twopi, osage
and patchwork and depends on GTS and GLib, are opened when an engine
is asked for that the plugins loaded so far do not provide.  If no
plugin could be found when the library was built, graphviz loads its
plugins as configured.

Graphviz keeps global state, so all calls are serialized.
*/

#ifdef HAVE_GVPLUGIN_DOT_LAYOUT
extern gvplugin_library_t gvplugin_dot_layout_LTX_library;
#endif

static const lt_symlist_t builtins[] =
{
#ifdef HAVE_GVPLUGIN_DOT_LAYOUT
  { "gvplugin_dot_layout_LTX_library", &gvplugin_dot_layout_LTX_library },
#endif
  { 0, 0 }
};

/* Plugins opened on demand: the file found at build time, its name and
the name of its plugin library symbol.  The file is searched for where
it is after relocating the installation (see open_plugin()) before the
path found at build time is used.
*/

typedef struct
{ const char *path;
  const char *name;
  const char *symbol;
  int	      tried;
} loadable_plugin;

static loadable_plugin loadable[] =
{
#ifdef GVPLUGIN_DOT_LAYOUT_PATH
  { GVPLUGIN_DOT_LAYOUT_PATH, GVPLUGIN_DOT_LAYOUT_NAME,
    "gvplugin_dot_layout_LTX_library", 0 },
#endif
#ifdef GVPLUGIN_NEATO_LAYOUT_PATH
  { GVPLUGIN_NEATO_LAYOUT_PATH, GVPLUGIN_NEATO_LAYOUT_NAME,
    "gvplugin_neato_layout_LTX_library", 0 },
#endif
  { NULL, NULL, NULL, 0 }
};

#if defined(HAVE_GVPLUGIN_DOT_LAYOUT) || \
    (defined(HAVE_DLFCN_H) && \
     (defined(GVPLUGIN_DOT_LAYOUT_PATH) || defined(GVPLUGIN_NEATO_LAYOUT_PATH)))
#define OWN_PLUGINS 1
#endif

static pthread_mutex_t gv_mutex = PTHREAD_MUTEX_INITIALIZER;
static GVC_t *gvc = NULL;

static functor_t FUNCTOR_layout4;
static functor_t FUNCTOR_cluster3;
static functor_t FUNCTOR_bb4;
static functor_t FUNCTOR_node2;
static functor_t FUNCTOR_edge2;
static functor_t FUNCTOR_point2;
static functor_t FUNCTOR_route4;
static functor_t FUNCTOR_error2;
static functor_t FUNCTOR_graphviz3;
static atom_t    ATOM_none;

		 /*******************************
		 *           MESSAGES		*
		 *******************************/

/* Graphviz reports errors and warnings through a callback that has no
closure argument.  The messages are collected in a buffer that is only
used while gv_mutex is held.
*/

static char  *msg_buf  = NULL;
static size_t msg_len  = 0;
static size_t msg_size = 0;

static int
collect_message(char *msg)
{ size_t len = strlen(msg);

  if ( msg_len+len+1 > msg_size )
  { size_t size = msg_size ? msg_size*2 : 256;
    char *buf;

    while ( size < msg_len+len+1 )
      size *= 2;
    if ( !(buf = realloc(msg_buf, size)) )
      return 0;
    msg_buf  = buf;
    msg_size = size;
  }
  memcpy(msg_buf+msg_len, msg, len+1);
  msg_len += len;

  return 0;
}

static void
clear_messages(void)
{ msg_len = 0;
  if ( msg_buf )
    msg_buf[0] = 0;
}

static const char *
messages(void)
{ return msg_buf ? msg_buf : "";
}

		 /*******************************
		 *            OUTPUT		*
		 *******************************/

static int
unify_point(term_t t, pointf p)
{ return PL_unify_term(t, PL_FUNCTOR, FUNCTOR_point2,
			    PL_FLOAT, p.x,
			    PL_FLOAT, p.y);
}

static int
unify_point_or_none(term_t t, int set, pointf p)
{ if ( set )
    return unify_point(t, p);
  return PL_unify_atom(t, ATOM_none);
}

/* route(Points, Start, End, Label) for an edge.  If graphviz merged
edges, the spline has more than one bezier; we use the first, as the
pos attribute in the JSON output does.
*/

static int
unify_route(term_t t, Agedge_t *e)
{ term_t av = PL_new_term_refs(4);
  splines *spl = ED_spl(e);
  textlabel_t *lbl = ED_label(e);

  if ( spl && spl->size > 0 )
  { bezier *bz = &spl->list[0];
    term_t tail = PL_copy_term_ref(av+0);
    term_t head = PL_new_term_ref();

    for(size_t i=0; i<bz->size; i++)
    { if ( !PL_unify_list(tail, head, tail) ||
	   !unify_point(head, bz->list[i]) )
	return FALSE;
    }
    if ( !PL_unify_nil(tail) ||
	 !unify_point_or_none(av+1, bz->sflag, bz->sp) ||
	 !unify_point_or_none(av+2, bz->eflag, bz->ep) )
      return FALSE;
  } else
  { if ( !PL_unify_nil(av+0) ||
	 !PL_unify_atom(av+1, ATOM_none) ||
	 !PL_unify_atom(av+2, ATOM_none) )
      return FALSE;
  }

  if ( !unify_point_or_none(av+3, lbl && lbl->set, lbl ? lbl->pos
							: (pointf){0,0}) )
    return FALSE;

  return PL_cons_functor_v(av+0, FUNCTOR_route4, av) &&
	 PL_unify(t, av+0);
}

static int
unify_bb(term_t t, boxf bb)
{ return PL_unify_term(t, PL_FUNCTOR, FUNCTOR_bb4,
			    PL_FLOAT, bb.LL.x, PL_FLOAT, bb.LL.y,
			    PL_FLOAT, bb.UR.x, PL_FLOAT, bb.UR.y);
}

/* Add cluster(Name, BB, Label) for the clusters below g to the list
clusters, outer clusters first.
*/

static int
unify_clusters(term_t clusters, Agraph_t *g)
{ term_t head = PL_new_term_ref();
  term_t tmp  = PL_new_term_ref();

  for(Agraph_t *sg = agfstsubg(g); sg; sg = agnxtsubg(sg))
  { const char *name = agnameof(sg);

    if ( strncmp(name, "cluster", 7) == 0 )
    { textlabel_t *lbl = GD_label(sg);

      if ( !PL_unify_list(clusters, head, clusters) ||
	   !PL_unify_functor(head, FUNCTOR_cluster3) ||
	   !PL_get_arg(1, head, tmp) ||
	   !PL_unify_chars(tmp, PL_ATOM|REP_UTF8, (size_t)-1, name) ||
	   !PL_get_arg(2, head, tmp) ||
	   !unify_bb(tmp, GD_bb(sg)) ||
	   !PL_get_arg(3, head, tmp) ||
	   !unify_point_or_none(tmp, lbl && lbl->set,
				lbl ? lbl->pos : (pointf){0,0}) )
	return FALSE;
    }
    if ( !unify_clusters(clusters, sg) )
      return FALSE;
  }

  return TRUE;
}

static int
unify_layout(term_t t, Agraph_t *g)
{ term_t av    = PL_new_term_refs(4);
  term_t nodes = PL_copy_term_ref(av+1);
  term_t edges = PL_copy_term_ref(av+2);
  term_t clusters = PL_copy_term_ref(av+3);
  term_t head  = PL_new_term_ref();
  term_t tmp   = PL_new_term_ref();
  boxf bb      = GD_bb(g);

  if ( !unify_bb(av+0, bb) )
    return FALSE;

  for(Agnode_t *n = agfstnode(g); n; n = agnxtnode(g, n))
  { if ( !PL_unify_list(nodes, head, nodes) ||
	 !PL_unify_functor(head, FUNCTOR_node2) ||
	 !PL_get_arg(1, head, tmp) ||
	 !PL_unify_chars(tmp, PL_ATOM|REP_UTF8, (size_t)-1, agnameof(n)) ||
	 !PL_get_arg(2, head, tmp) ||
	 !unify_point(tmp, ND_coord(n)) )
      return FALSE;

    for(Agedge_t *e = agfstout(g, n); e; e = agnxtout(g, e))
    { char *id = agget(e, "id");

      if ( !PL_unify_list(edges, head, edges) ||
	   !PL_unify_functor(head, FUNCTOR_edge2) ||
	   !PL_get_arg(1, head, tmp) ||
	   !PL_unify_chars(tmp, PL_ATOM|REP_UTF8, (size_t)-1, id ? id : "") ||
	   !PL_get_arg(2, head, tmp) ||
	   !unify_route(tmp, e) )
	return FALSE;
    }
  }

  return ( PL_unify_nil(nodes) &&
	   PL_unify_nil(edges) &&
	   unify_clusters(clusters, g) &&
	   PL_unify_nil(clusters) &&
	   PL_cons_functor_v(head, FUNCTOR_layout4, av) &&
	   PL_unify(t, head) );
}

static int
graphviz_error(const char *engine, const char *what)
{ term_t ex;

  return ( (ex=PL_new_term_ref()) &&
	   PL_unify_term(ex,
			 PL_FUNCTOR, FUNCTOR_error2,
			   PL_FUNCTOR, FUNCTOR_graphviz3,
			     PL_UTF8_CHARS, engine,
			     PL_UTF8_CHARS, what,
			     PL_UTF8_STRING, messages(),
			   PL_VARIABLE) &&
	   PL_raise_exception(ex) );
}

		 /*******************************
		 *           PREDICATE		*
		 *******************************/

/* Create the graphviz context if needed.  Must be called with gv_mutex
held.
*/

static GVC_t *
gv_context(void)
{ if ( !gvc )
  { agseterrf(collect_message);
#ifdef OWN_PLUGINS
    gvc = gvContextPlugins(builtins, 0);
#else
    gvc = gvContext();
#endif
  }

  return gvc;
}

#if defined(OWN_PLUGINS) && defined(HAVE_DLFCN_H)
/* dir_of(addr, buf, size) puts the directory of the file that holds
addr in buf.
*/

static int
dir_of(const void *addr, char *buf, size_t size)
{ Dl_info info;
  const char *sep;

  if ( dladdr(addr, &info) && info.dli_fname &&
       (sep = strrchr(info.dli_fname, '/')) &&
       (size_t)(sep-info.dli_fname) < size )
  { memcpy(buf, info.dli_fname, sep-info.dli_fname);
    buf[sep-info.dli_fname] = 0;
    return TRUE;
  }

  return FALSE;
}

static void *
try_open(const char *dir, const char *sub, const char *name)
{ char path[PATH_MAX];

  if ( snprintf(path, sizeof(path), "%s%s/%s", dir, sub, name) <
       (int)sizeof(path) )
    return dlopen(path, RTLD_NOW|RTLD_LOCAL);

  return NULL;
}

/* Open a plugin.  The installation may have been moved since it was
built, e.g., into a macOS application bundle, so we try in order

  - The directory holding this library.  Bundles that ship their own
    graphviz install the plugins there.
  - The graphviz directory below the directory holding libgvc, the
    layout of a graphviz installation.
  - The directory holding libgvc.
  - The path found when this library was built.
*/

static void *
open_plugin(const loadable_plugin *p)
{ char dir[PATH_MAX];
  void *h;

  if ( dir_of((void*)open_plugin, dir, sizeof(dir)) &&
       (h = try_open(dir, "", p->name)) )
    return h;
  if ( dir_of((void*)gvContext, dir, sizeof(dir)) &&
       ((h = try_open(dir, "/graphviz", p->name)) ||
	(h = try_open(dir, "", p->name))) )
    return h;

  return dlopen(p->path, RTLD_NOW|RTLD_LOCAL);
}
#endif

/* Open the plugins that were not tried yet.  Returns the number of
plugins added.  A plugin that cannot be opened is tried only once.
*/

static int
load_plugins(void)
{ int added = 0;

#if defined(OWN_PLUGINS) && defined(HAVE_DLFCN_H)
  for(loadable_plugin *p = loadable; p->path; p++)
  { void *h;
    gvplugin_library_t *lib;

    if ( p->tried )
      continue;
    p->tried = TRUE;
    if ( (h = open_plugin(p)) &&
	 (lib = dlsym(h, p->symbol)) )
    { gvAddLibrary(gvc, lib);
      added++;
    } else if ( h )
    { dlclose(h);
    }
  }
#endif

  return added;
}

static int
has_engine(const char *engine)
{ char **list;
  int count = 0;
  int found = FALSE;

  if ( (list = gvPluginList(gvc, "layout", &count)) )
  { for(int i=0; i<count; i++)
    { if ( strcmp(list[i], engine) == 0 )
	found = TRUE;
      free(list[i]);
    }
    free(list);
  }

  return found;
}

/* Make sure engine is available, opening plugins if needed.  Must be
called with gv_mutex held, after gv_context().
*/

static int
ensure_engine(const char *engine)
{ if ( has_engine(engine) )
    return TRUE;
  if ( load_plugins() > 0 )
    return has_engine(engine);
  return FALSE;
}

/** graphviz_engines(-Engines)
*/

static foreign_t
pl_graphviz_engines(term_t Engines)
{ term_t tail = PL_copy_term_ref(Engines);
  term_t head = PL_new_term_ref();
  char **list = NULL;
  int count = 0;
  int rc = TRUE;

  pthread_mutex_lock(&gv_mutex);
  clear_messages();
  if ( gv_context() )
  { load_plugins();
    list = gvPluginList(gvc, "layout", &count);
  }
  pthread_mutex_unlock(&gv_mutex);

  for(int i=0; i<count; i++)
  { if ( rc )
      rc = ( PL_unify_list(tail, head, tail) &&
	     PL_unify_chars(head, PL_ATOM|REP_UTF8, (size_t)-1, list[i]) );
    free(list[i]);
  }
  free(list);

  return rc && PL_unify_nil(tail);
}

/** graphviz_has_engine(+Engine)
*/

static foreign_t
pl_graphviz_has_engine(term_t Engine)
{ char *engine;
  int rc;

  if ( !PL_get_chars(Engine, &engine, CVT_ATOM|CVT_STRING|CVT_EXCEPTION|
				      REP_UTF8) )
    return FALSE;

  pthread_mutex_lock(&gv_mutex);
  clear_messages();
  rc = ( gv_context() && ensure_engine(engine) );
  pthread_mutex_unlock(&gv_mutex);

  return rc;
}

/** graphviz_layout_dot(+Engine, +Dot, -Layout, -Messages)
*/

static foreign_t
pl_graphviz_layout_dot(term_t Engine, term_t Dot, term_t Layout,
		       term_t Messages)
{ char *engine, *dot;
  Agraph_t *g = NULL;
  int rc = FALSE;
  int laid_out = FALSE;

  if ( !PL_get_chars(Engine, &engine, CVT_ATOM|CVT_STRING|CVT_EXCEPTION|
				      REP_UTF8) ||
       !PL_get_chars(Dot, &dot, CVT_ATOM|CVT_STRING|CVT_LIST|CVT_EXCEPTION|
				REP_UTF8|BUF_MALLOC) )
    return FALSE;

  pthread_mutex_lock(&gv_mutex);
  clear_messages();
  if ( !gv_context() )
  { rc = graphviz_error(engine, "context");
    goto out;
  }

  ensure_engine(engine);		/* gvLayout() reports if not */
  if ( !(g = agmemread(dot)) )
  { rc = graphviz_error(engine, "syntax");
    goto out;
  }
  if ( gvLayout(gvc, g, engine) != 0 )
  { rc = graphviz_error(engine, "layout");
    goto out;
  }
  laid_out = TRUE;

  rc = ( unify_layout(Layout, g) &&
	 PL_unify_chars(Messages, PL_STRING|REP_UTF8, (size_t)-1,
			messages()) );

out:
  if ( laid_out )
    gvFreeLayout(gvc, g);
  if ( g )
    agclose(g);
  pthread_mutex_unlock(&gv_mutex);
  PL_free(dot);

  return rc;
}

#define MKFUNCTOR(n, a) \
	FUNCTOR_ ## n ## a = PL_new_functor(PL_new_atom(#n), a)

install_t
install_graphviz4pl(void)
{ MKFUNCTOR(layout, 4);
  MKFUNCTOR(cluster, 3);
  MKFUNCTOR(bb, 4);
  MKFUNCTOR(node, 2);
  MKFUNCTOR(edge, 2);
  MKFUNCTOR(point, 2);
  MKFUNCTOR(route, 4);
  MKFUNCTOR(error, 2);
  MKFUNCTOR(graphviz, 3);
  ATOM_none = PL_new_atom("none");

  PL_register_foreign("graphviz_layout_dot", 4, pl_graphviz_layout_dot, 0);
  PL_register_foreign("graphviz_engines",    1, pl_graphviz_engines,    0);
  PL_register_foreign("graphviz_has_engine", 1, pl_graphviz_has_engine, 0);
}
