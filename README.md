# Graphviz layout for SWI-Prolog

This package provides library(graphviz), a binding to the layout part
of the [graphviz](https://graphviz.org) libraries.  It lays out a graph
given in the DOT language and returns the positions of the nodes and the
routes of the edges as a Prolog term.  Nothing is rendered: the nodes
and labels are drawn by the application, which tells graphviz only their
size.  This is used by library(graphviz_layout) of xpce.

The package needs the graphviz development files (`libgvc`, `libcgraph`
and `libcdt`).  If the dot layout plugin can be linked, it is the only
plugin used, and graphviz loads no plugins dynamically.

Graphviz is licensed under the Eclipse Public License 1.0.
