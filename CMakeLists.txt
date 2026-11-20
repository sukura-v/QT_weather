cmake_minimum_required(VERSION 3.19)
project(Weather LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Network)

qt_standard_project_setup()

qt_add_executable(Weather
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    adddialog.h adddialog.cpp adddialog.ui
    weatherworker.h weatherworker.cpp
    detaildialog.h detaildialog.cpp detaildialog.ui
    settingdialog.h settingdialog.cpp settingdialog.ui
)

target_link_libraries(Weather
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Sql
        Qt::Network
)

install(TARGETS Weather
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Weather
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
