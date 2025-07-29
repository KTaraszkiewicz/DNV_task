
cmake_minimum_required(VERSION 3.16)
project(STLViewer)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure correct __cplusplus value for Qt on MSVC
if (MSVC)
    add_compile_options(/Zc:__cplusplus /permissive-)
endif()

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)


# Add source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/glwidget.cpp
    src/stlloader.cpp
    src/camera.cpp
)

set(HEADERS
    src/mainwindow.h
    src/glwidget.h
    src/stlloader.h
    src/camera.h
)



set(UIS
    src/mainwindow.ui
)

set(RESOURCES
    resources/resources.qrc
)

qt6_wrap_ui(UI_HEADERS ${UIS})
qt_add_resources(RCC_SOURCES ${RESOURCES})

# Create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_HEADERS}
    ${RCC_SOURCES}
)

# Link Qt6 libraries
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

# Include paths
target_include_directories(${PROJECT_NAME} PRIVATE src)
