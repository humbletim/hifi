#
#  Copyright 2017 High Fidelity, Inc.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#

set(TARGET_NAME hifiLeopoly)
include_hifi_library_headers(shared animation fbx model gpu networking avatars entities audio script-engine)
#add_dependency_external_projects(leopoly)
setup_hifi_plugin()
link_hifi_libraries(shared entities model gpu model-networking networking plugins)
install_beside_console()
