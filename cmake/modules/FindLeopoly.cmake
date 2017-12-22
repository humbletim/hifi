#
#  FindLeopoly.cmake
#
#  Try to find Leopoly SculptLib tools library and include path.
#  Once done this will define
#
#  LEOPOLY_FOUND
#  LEOPOLY_INCLUDE_DIRS
#  LEOPOLY_LIBRARIES
#  LEOPOLY_DLL_PATH
#
#  Copyright 2017 High Fidelity, Inc.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#

include("${MACRO_DIR}/HifiLibrarySearchHints.cmake")
hifi_library_search_hints("leopoly")

find_path(LEOPOLY_INCLUDE_DIRS Leopoly/SculptLib.h PATH_SUFFIXES include HINTS ${LEOPOLY_SEARCH_DIRS})

include(FindPackageHandleStandardArgs)

find_library(LEOPOLY_LIBRARY_RELEASE SculptLib PATH_SUFFIXES "lib" "lib/win64" HINTS ${LEOPOLY_SEARCH_DIRS})
find_library(LEOPOLY_LIBRARY_DEBUG SculptLib PATH_SUFFIXES "lib" "lib/win64" HINTS ${LEOPOLY_SEARCH_DIRS})

include(SelectLibraryConfigurations)
select_library_configurations(leopoly)

if (WIN32)
  find_path(LEOPOLY_DLL_PATH SculptLib.dll PATH_SUFFIXES "lib/win64" HINTS ${LEOPOLY_SEARCH_DIRS})
  find_package_handle_standard_args(Leopoly DEFAULT_MSG LEOPOLY_INCLUDE_DIRS LEOPOLY_LIBRARIES LEOPOLY_DLL_PATH)
else ()
  find_package_handle_standard_args(Leopoly DEFAULT_MSG LEOPOLY_INCLUDE_DIRS LEOPOLY_LIBRARIES)
endif ()
