cmake_minimum_required(VERSION 3.24)
project(FaceRelay)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Network)
qt_standard_project_setup()

set(TS_FILES
    lang/zh_CN.ts
    lang/en_US.ts
)

set(PROJECT_SOURCES
    main.cpp
    ui/mainwindow.cpp ui/mainwindow.h ui/mainwindow.ui)

qt_add_executable(FaceRelay
    icon/icon.rc
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

set_target_properties(FaceRelay
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(FaceRelay PRIVATE Qt6::Widgets Qt6::Network)

qt_add_translations(FaceRelay TS_FILES ${TS_FILES})

find_package(ixwebsocket CONFIG REQUIRED)
target_link_libraries(FaceRelay PRIVATE ixwebsocket::ixwebsocket)

find_package(simdjson CONFIG REQUIRED)
target_link_libraries(FaceRelay PRIVATE simdjson::simdjson)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBIMOBILEDEVICE REQUIRED IMPORTED_TARGET libimobiledevice-1.0)
target_link_libraries(FaceRelay PUBLIC PkgConfig::LIBIMOBILEDEVICE)

set_target_properties(FaceRelay PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Install
if (${CMAKE_BUILD_TYPE} MATCHES "Debug" AND WIN32)
    install(FILES "C:/Windows/System32/ucrtbased.dll" DESTINATION bin)
endif()
install(DIRECTORY "${CMAKE_BINARY_DIR}/bin/" DESTINATION ${CMAKE_INSTALL_BINDIR} FILES_MATCHING PATTERN "*.dll")
install(TARGETS FaceRelay
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET FaceRelay
    FILENAME_VARIABLE DEPLOY_SCRIPT
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${DEPLOY_SCRIPT})
qt_finalize_executable(FaceRelay)

file(WRITE "${CMAKE_BINARY_DIR}/bin/qt.conf" "[Paths]
Prefix = ..")
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    file(COPY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/Qt6/plugins/" DESTINATION "${CMAKE_BINARY_DIR}/plugins/")
else()
    file(COPY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/Qt6/plugins/" DESTINATION "${CMAKE_BINARY_DIR}/plugins/")
endif()