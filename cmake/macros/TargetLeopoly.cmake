#
#  Copyright 2017 High Fidelity, Inc.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#
macro(TARGET_LEOPOLY)
    add_dependency_external_projects(leopoly)

    find_package(LEOPOLY REQUIRED)

    add_paths_to_fixup_libs(${LEOPOLY_DLL_PATH})

    target_include_directories(${TARGET_NAME} SYSTEM PRIVATE ${LEOPOLY_INCLUDE_DIRS})
    target_link_libraries(${TARGET_NAME} ${LEOPOLY_LIBRARIES})
endmacro()
