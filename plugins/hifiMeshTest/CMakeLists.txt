#
#  Copyright 2018 High Fidelity, Inc.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#

set(TARGET_NAME hifiMeshTest)
AUTOSCRIBE_SHADER_LIB(gpu graphics render-utils)
setup_hifi_plugin(Script Network)
link_hifi_libraries(shared plugins graphics graphics-scripting model-networking networking image octree ktx entities fbx animation render render-utils object-plugins)
include_hifi_library_headers(fbx ktx image graphics-scripting object-plugins render-utils model-networking networking entities octree)
target_tbb()
install_beside_console()
