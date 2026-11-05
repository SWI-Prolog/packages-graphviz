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


:- module(test_graphviz,
          [ test_graphviz/0
          ]).
:- use_module(library(plunit)).
:- use_module(library(lists)).
:- use_module(library(graphviz)).

test_graphviz :-
    run_tests([graphviz]).

:- begin_tests(graphviz).

test(nodes, Names == [a,b]) :-
    graphviz_layout_dot(dot,
                        "digraph { node [shape=box fixedsize=true label=\"\"]; \c
                         a [width=1 height=0.5]; b [width=1 height=0.5]; \c
                         a -> b [id=e0]; }",
                        layout(bb(_,_,_,_), Nodes, _), _),
    findall(N, member(node(N,_), Nodes), Names0),
    msort(Names0, Names).
test(rank, true(YA > YB)) :-            % graphviz Y grows upwards
    graphviz_layout_dot(dot, "digraph { a -> b; }",
                        layout(_, Nodes, _), _),
    memberchk(node(a, point(_,YA)), Nodes),
    memberchk(node(b, point(_,YB)), Nodes).
test(edge, true) :-
    graphviz_layout_dot(dot, "digraph { a -> b [id=e0]; }",
                        layout(_, _, [edge(e0, Route)]), _),
    Route = route(Points, none, point(_,_), none),
    length(Points, Len),
    Len mod 3 =:= 1.
test(label, true) :-
    graphviz_layout_dot(dot,
                        "digraph { a -> b [label=<<TABLE FIXEDSIZE=\"TRUE\" \c
                         WIDTH=\"30\" HEIGHT=\"12\"><TR><TD></TD></TR>\c
                         </TABLE>>]; }",
                        layout(_, _, [edge(_, route(_, _, _, point(_,_)))]), _).
test(syntax, error(graphviz(dot, syntax, _))) :-
    graphviz_layout_dot(dot, "digraph { a -> ; }", _, _).

:- end_tests(graphviz).
