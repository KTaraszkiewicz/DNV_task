cmake_minimum_required(VERSION 3.16)
project(STLViewer VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_PREFIX_PATH "G:/QT6/6.9.1/msvc2022_64")

# Enable Qt MOC (Meta-Object Compiler)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Set Qt policy for Qt6
if(POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()

# Find Qt6 - prefer Qt6 over Qt5
find_package(Qt6 QUIET COMPONENTS Core Widgets OpenGL OpenGLWidgets Gui)

if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    message(STATUS "Using Qt6")
    set(QT_LIBRARIES Qt6::Core Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Gui)
else()
    # Fallback to Qt5 if Qt6 not found
    message(STATUS "Qt6 not found, trying Qt5...")
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets OpenGL Gui)
    set(QT_VERSION_MAJOR 5)
    message(STATUS "Using Qt5")
    set(QT_LIBRARIES Qt5::Core Qt5::Widgets Qt5::OpenGL Qt5::Gui)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/glwidget.cpp
    src/camera.cpp
    src/stlloader.cpp
)

# Header files
set(HEADERS
    src/mainwindow.h
    src/glwidget.h
    src/camera.h
    src/stlloader.h
)

# UI files
set(UI_FILES
    src/mainwindow.ui
)

# Check if source files exist
foreach(source ${SOURCES})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${source})
        message(WARNING "Source file not found: ${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    endif()
endforeach()

# Create executable
add_executable(STLViewer ${SOURCES} ${HEADERS} ${UI_FILES})

# Link Qt libraries
target_link_libraries(STLViewer ${QT_LIBRARIES})

# Link native OpenGL for Windows
if(WIN32)
    target_link_libraries(STLViewer opengl32)
endif()

# Include directories
target_include_directories(STLViewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Set target properties
set_target_properties(STLViewer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Compiler-specific options
if(MSVC)
    target_compile_options(STLViewer PRIVATE /W3 /Zc:__cplusplus /permissive-)
    # Disable some MSVC warnings
    target_compile_definitions(STLViewer PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
else()
    target_compile_options(STLViewer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Debug/Release configurations
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(STLViewer PRIVATE DEBUG)
else()
    target_compile_definitions(STLViewer PRIVATE NDEBUG)
endif()

# Platform-specific settings
if(WIN32 AND QT_VERSION_MAJOR EQUAL 6)
    # Windows specific settings for Qt6
    set_target_properties(STLViewer PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
    
    # Qt6 deployment
    find_program(QT_WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${CMAKE_PREFIX_PATH}/bin)
    if(QT_WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET STLViewer POST_BUILD
            COMMAND ${QT_WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:STLViewer>
            COMMENT "Deploying Qt libraries"
        )
    endif()
endif()

# Print configuration information
message(STATUS "STLViewer Configuration:")
message(STATUS "  Qt Version: ${QT_VERSION_MAJOR}")
message(STATUS "  Qt Path: ${CMAKE_PREFIX_PATH}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")