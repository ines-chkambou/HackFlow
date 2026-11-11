cmake_minimum_required(VERSION 3.16)

project(hackflow VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Concurrent Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Concurrent Sql)
find_package(Qt${QT_VERSION_MAJOR} QUIET OPTIONAL_COMPONENTS Pdf)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        connection.cpp
        connection.h
        employe.cpp
        employe.h
        participantscontroller.cpp
        participantscontroller.h
        jurycontroller.cpp
        jurycontroller.h
        scrollhelpers.h
        projectspage.cpp
        projectspage.h
        resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(hackflow
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(hackflow SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(hackflow
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(hackflow PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Concurrent Qt${QT_VERSION_MAJOR}::Sql)
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    target_link_libraries(hackflow PRIVATE Qt6::GuiPrivate)
    if(TARGET Qt6::Pdf AND ${QT_VERSION} VERSION_GREATER_EQUAL 6.6)
        target_link_libraries(hackflow PRIVATE Qt6::Pdf)
        target_compile_definitions(hackflow PRIVATE HAVE_QTPDF)
    endif()
endif()
target_include_directories(hackflow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.hackflow)
endif()
set_target_properties(hackflow PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS hackflow
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(hackflow)
endif()
