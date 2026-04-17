cmake_minimum_required(VERSION 3.16)
project(QHugo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Gui QuickControls2 WebEngineQuick)

# Build Go Backend
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/backend/backend.a ${CMAKE_CURRENT_SOURCE_DIR}/backend/backend.h
    COMMAND go mod tidy && go build -buildmode=c-archive -o backend.a backend.go
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/backend/backend.go
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/backend
    COMMENT "Building Go backend..."
)

add_custom_target(go_backend DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/backend/backend.a)

qt_standard_project_setup()

# Include highlighter directory
include_directories(highlighter)

qt_add_executable(appQHugo
    main.cpp
    filecontroller.cpp
    filecontroller.h
    lspclient.cpp
    lspclient.h
    markdownhighlighter.cpp
    markdownhighlighter.h
    # Add Library Files
    highlighter/languagedata.cpp
    highlighter/languagedata.h
    highlighter/qsourcehighliterthemes.cpp
    highlighter/qsourcehighliterthemes.h
)

qt_add_qml_module(appQHugo
    URI QHugo
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Editor.qml
        qml/Sidebar.qml
        qml/FuzzyFinder.qml
)

target_link_libraries(appQHugo
    PRIVATE
    Qt6::Core Qt6::Quick Qt6::Gui Qt6::QuickControls2 Qt6::WebEngineQuick
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/backend.a
    Threads::Threads
    ${CMAKE_DL_LIBS} 
)

target_include_directories(appQHugo PRIVATE backend)
add_dependencies(appQHugo go_backend)
