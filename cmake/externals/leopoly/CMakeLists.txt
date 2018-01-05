include(ExternalProject)
include(SelectLibraryConfigurations)

set(EXTERNAL_NAME leopoly)

string(TOUPPER ${EXTERNAL_NAME} EXTERNAL_NAME_UPPER)

if (WIN32)

  ExternalProject_Add(
        ${EXTERNAL_NAME}
        URL file://c:/src/TODO-20171201-LeopolySculptLib.zip #https://s3.amazonaws.com/hifi-public/dependencies/TODO-20171201-LeopolySculptLib.zip
        # URL_MD5 nnnnnnnnnnnnnnnn
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD 1
  )

  # Hide this external target (for ide users)
  set_target_properties(${EXTERNAL_NAME} PROPERTIES FOLDER "hidden/externals")
  ExternalProject_Get_Property(${EXTERNAL_NAME} SOURCE_DIR)
  set(${EXTERNAL_NAME_UPPER}_INCLUDE_DIRS ${SOURCE_DIR}/include CACHE TYPE INTERNAL)
  set(${EXTERNAL_NAME_UPPER}_LIBRARIES ${SOURCE_DIR}/lib/win64/SculptLib.lib CACHE TYPE INTERNAL)
  set(${EXTERNAL_NAME_UPPER}_DLL_PATH "${SOURCE_DIR}/lib/win64" CACHE TYPE INTERNAL)
endif()

message(STATUS "${EXTERNAL_NAME_UPPER}_INCLUDE_DIRS = ${${EXTERNAL_NAME_UPPER}_INCLUDE_DIRS}")
message(STATUS "${EXTERNAL_NAME_UPPER}_LIBRARIES = ${${EXTERNAL_NAME_UPPER}_LIBRARIES}")
message(STATUS "${EXTERNAL_NAME_UPPER}_DLL_PATH = ${${EXTERNAL_NAME_UPPER}_DLL_PATH}")
