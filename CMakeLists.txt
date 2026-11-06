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
if(WIN32)
# On Windows, find_library() finds the import library.  The dot plugin
# is linked through it.  The others are opened with LoadLibrary(), for
# which we need the DLL the import library is for.
  if(GRAPHVIZ_DOT_LAYOUT_LIBRARY)
    set(HAVE_GVPLUGIN_DOT_LAYOUT 1)
    list(APPEND GRAPHVIZ_LIBS ${GRAPHVIZ_DOT_LAYOUT_LIBRARY})
  endif()
  if(GRAPHVIZ_NEATO_LAYOUT_LIBRARY)
    get_filename_component(dir ${GRAPHVIZ_NEATO_LAYOUT_LIBRARY} DIRECTORY)
    file(STRINGS ${GRAPHVIZ_NEATO_LAYOUT_LIBRARY} dlls
	 REGEX "^libgvplugin_neato_layout[-0-9]*\\.dll$")
    if(dlls)
      list(GET dlls 0 dll)
      get_filename_component(GVPLUGIN_NEATO_LAYOUT_PATH
			     ${dir}/../bin/${dll} ABSOLUTE)
      install_dll(${GVPLUGIN_NEATO_LAYOUT_PATH})
    endif()
  endif()
else()
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
endif()

# The plugins that are opened at runtime are found by the name of the
# file the link points at, as an installation holds that file.
set(GRAPHVIZ_RUNTIME_PLUGINS)
foreach(plugin DOT_LAYOUT NEATO_LAYOUT)
  if(GVPLUGIN_${plugin}_PATH)
    get_filename_component(real ${GVPLUGIN_${plugin}_PATH} REALPATH)
    get_filename_component(GVPLUGIN_${plugin}_NAME ${real} NAME)
    list(APPEND GRAPHVIZ_RUNTIME_PLUGINS ${real})
  endif()
endforeach()

# A macOS bundle or framework ships its own graphviz.  The plugins it
# opens at runtime are not linked, so the bundle fixup would not find
# them: install them next to graphviz4pl, where the fixup processes
# them as it does our plugins and where graphviz4pl looks first.
if((BUILD_MACOS_BUNDLE OR BUILD_MACOS_FRAMEWORK) AND GRAPHVIZ_RUNTIME_PLUGINS)
  install(FILES ${GRAPHVIZ_RUNTIME_PLUGINS}
	  DESTINATION ${SWIPL_INSTALL_MODULES}
	  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
		      GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
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

install_dll(${GRAPHVIZ_LIBS})

test_libs(
    graphviz
    PACKAGES plunit)

pkg_doc(graphviz
	SECTION
	SOURCE graphviz.pl libgraphviz.tex)

endif(GRAPHVIZ_FOUND)
