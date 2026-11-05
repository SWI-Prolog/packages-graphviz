cmake_minimum_required(VERSION 3.10)
project(swipl-graphviz)

include("../cmake/PrologPackage.cmake")
find_package(Graphviz)

if(GRAPHVIZ_FOUND)

set(GRAPHVIZ_LIBS ${GRAPHVIZ_LIBRARIES})
if(GRAPHVIZ_DOT_LAYOUT_LIBRARY)
  set(HAVE_GVPLUGIN_DOT_LAYOUT 1)
  list(APPEND GRAPHVIZ_LIBS ${GRAPHVIZ_DOT_LAYOUT_LIBRARY})
endif()

configure_file(config.h.cmake config.h)

swipl_plugin(
    graphviz
    MODULE graphviz4pl
    C_SOURCES graphviz4pl.c
    C_LIBS ${GRAPHVIZ_LIBS}
    C_INCLUDE_DIR ${GRAPHVIZ_INCLUDE_DIR}
    PL_LIBS graphviz.pl)

test_libs(
    graphviz
    PACKAGES plunit)

endif(GRAPHVIZ_FOUND)
