# Find the graphviz libraries needed for layout
#
#   GRAPHVIZ_FOUND
#   GRAPHVIZ_INCLUDE_DIR       Directory holding gvc.h
#   GRAPHVIZ_LIBRARIES         gvc, cgraph and cdt
#   GRAPHVIZ_DOT_LAYOUT_LIBRARY    The dot layout plugin, if found
#   GRAPHVIZ_NEATO_LAYOUT_LIBRARY  The neato layout plugin, if found
#
# Distributions may only ship the versioned plugins (.so.6), which are
# not found under their plain name.

find_package(PkgConfig QUIET)
pkg_check_modules(PC_GRAPHVIZ QUIET libgvc)

find_path(GRAPHVIZ_INCLUDE_DIR
	  NAMES gvc.h
	  HINTS ${PC_GRAPHVIZ_INCLUDEDIR}
		${PC_GRAPHVIZ_INCLUDE_DIRS}
	  PATH_SUFFIXES graphviz)

foreach(lib gvc cgraph cdt)
  string(TOUPPER ${lib} LIB)
  find_library(GRAPHVIZ_${LIB}_LIBRARY
	       NAMES ${lib}
	       HINTS ${PC_GRAPHVIZ_LIBDIR}
		     ${PC_GRAPHVIZ_LIBRARY_DIRS})
endforeach()

foreach(plugin dot_layout neato_layout)
  string(TOUPPER ${plugin} PLUGIN)
  find_library(GRAPHVIZ_${PLUGIN}_LIBRARY
	       NAMES gvplugin_${plugin}
		     libgvplugin_${plugin}.so.6
	       HINTS ${PC_GRAPHVIZ_LIBDIR}
		     ${PC_GRAPHVIZ_LIBRARY_DIRS}
	       PATH_SUFFIXES graphviz)
endforeach()

set(GRAPHVIZ_LIBRARIES
    ${GRAPHVIZ_GVC_LIBRARY}
    ${GRAPHVIZ_CGRAPH_LIBRARY}
    ${GRAPHVIZ_CDT_LIBRARY})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    Graphviz
    REQUIRED_VARS GRAPHVIZ_INCLUDE_DIR
		  GRAPHVIZ_GVC_LIBRARY
		  GRAPHVIZ_CGRAPH_LIBRARY
		  GRAPHVIZ_CDT_LIBRARY)

mark_as_advanced(GRAPHVIZ_INCLUDE_DIR
		 GRAPHVIZ_GVC_LIBRARY
		 GRAPHVIZ_CGRAPH_LIBRARY
		 GRAPHVIZ_CDT_LIBRARY
		 GRAPHVIZ_DOT_LAYOUT_LIBRARY
		 GRAPHVIZ_NEATO_LAYOUT_LIBRARY)
