\documentclass[11pt]{article}
\usepackage{times}
\usepackage{pl}
\usepackage{html}
\sloppy
\makeindex

\onefile
\htmloutput{.}					% Output directory
\htmlmainfile{graphviz}				% Main document file
\bodycolor{white}				% Page colour

\begin{document}

\title{SWI-Prolog Graphviz layout library}
\author{Jan Wielemaker \\
	SWI-Prolog Solutions b.v. \\
	The Netherlands \\
	E-mail: \email{jan@swi-prolog.org}}

\maketitle

\begin{abstract}
This package provides \pllib{graphviz}, a binding to the layout part of
the \href{https://graphviz.org}{Graphviz} libraries.  It lays out a
graph given in the DOT language and returns the positions of the nodes
and the routes of the edges as a Prolog term.  Nothing is rendered: the
nodes and labels are drawn by the application, which tells Graphviz only
their size.  It is used by \pllib{graphviz_layout} of XPCE.
\end{abstract}

\pagebreak
\tableofcontents

\vfill
\vfill

\newpage

\input{libgraphviz.tex}

\section{License}
\label{sec:graphviz-license}

The Prolog and C sources of this package are licensed under the
Simplified BSD license, as the rest of SWI-Prolog.  Graphviz, which it
links to, is licensed under the
\href{https://www.eclipse.org/legal/epl-v10.html}{Eclipse Public License
1.0}.  Loading \pllib{graphviz} registers this license, such that
license/0 reports it.

\printindex

\end{document}
