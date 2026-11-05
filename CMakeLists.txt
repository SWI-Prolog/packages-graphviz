cmake_minimum_required(VERSION 3.10)
project(swipl-graphviz)

include("../cmake/PrologPackage.cmake")
find_package(Graphviz)

if(GRAPHVIZ_FOUND)

include(CheckIncludeFile)
check_include_file(dlfcn.h HAVE_DLFCN_H)

# The dot layout plugin is linked if it can be.  Otherwise it is opened
# at runtime, as are the other layout plugins.
set(GRAPHVIZ_LIBS ${GRAPHVIZ_LIBRARIES})
if(GRAPHVIZ_DOT_LAYOUT_LIBRARY AND
   NOT GRAPHVIZ_DOT_LAYOUT_LIBRARY MATCHES "\\.so\\.[0-9]+$")
  set(HAVE_GVPLUGIN_DOT_LAYOUT 1)
  list(APPEND GRAPHVIZ_LIBS ${GRAPHVIZ_DOT_LAYOUT_LIBRARY})
elseif(GRAPHVIZ_DOT_LAYOUT_LIBRARY)
  set(GVPLUGIN_DOT_LAYOUT_PATH ${GRAPHVIZ_DOT_LAYOUT_LIBRARY})
endif()
if(GRAPHVIZ_NEATO_LAYOUT_LIBRARY)
  set(GVPLUGIN_NEATO_LAYOUT_PATH ${GRAPHVIZ_NEATO_LAYOUT_LIBRARY})
endif()
if(HAVE_DLFCN_H)
  list(APPEND GRAPHVIZ_LIBS ${CMAKE_DL_LIBS})
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
