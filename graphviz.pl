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


:- module(graphviz,
          [ graphviz_layout_dot/4       % +Engine, +Dot, -Layout, -Messages
          ]).
:- use_foreign_library(foreign(graphviz4pl)).

/** <module> Graphviz layout library

This library calls the layout functions of the graphviz libraries.  It
only uses the layout part of graphviz: there is no rendering and no
text layout.  It is intended for laying out graphs whose nodes and
labels are drawn by the application, which tells graphviz only their
size.  See library(graphviz_layout) of xpce.

If possible, the binding is linked with the dot layout plugin and no
other plugins are loaded.  Otherwise the plugins are loaded as graphviz
is configured.
*/

%!  graphviz_layout_dot(+Engine, +Dot, -Layout, -Messages) is det.
%
%   Lay out Dot, a graph in the DOT language as text, using the layout
%   engine Engine, normally `dot`.  Layout is a term
%
%       layout(bb(X0,Y0,X1,Y1), Nodes, Edges, Clusters)
%
%   in graphviz coordinates: points, with Y growing upwards.
%
%     - Nodes is a list of node(Name, point(X,Y)), the centre of the
%       node named Name in Dot.
%     - Edges is a list of edge(Id, Route), where Id is the `id`
%       attribute of the edge (an atom, '' if not set) and Route is
%       route(Points, Start, End, Label).  Points is the list of
%       control points of a cubic B-spline.  Start and End are `none` or
%       the tip of the arrow at that end.  Label is `none` or the centre
%       of the label.
%     - Clusters is a list of cluster(Name, bb(X0,Y0,X1,Y1), Label) for
%       the subgraphs whose name starts with `cluster`, outer clusters
%       before the clusters they hold.  Label is `none` or the centre
%       of the label.
%
%   Messages is a string holding the warnings of graphviz.
%
%   @error graphviz(Engine, What, Message) if the graph cannot be laid
%   out.  What is one of `syntax` or `layout`.

:- multifile prolog:error_message//1.

prolog:error_message(graphviz(Engine, What, Message)) -->
    { atom(What) },
    [ 'graphviz layout "~w" failed (~w)'-[Engine, What] ],
    (   { Message == "" }
    ->  []
    ;   { split_string(Message, "", " \t\n", [Msg]) },
        [ nl, '~s'-[Msg] ]
    ).
