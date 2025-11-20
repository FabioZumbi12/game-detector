#  OBS Game Detector - CMake Build Configuration
#  This CMake configuration file sets up the build process for the OBS Game Detector plugin.
#  It includes options for enabling the obs-frontend-api and Qt functionalities,
#  and specifies custom output directories for the built plugin and its associated data.
#  
#  If the build fail (the folder build_x64 don't exists), run on terminal: cmake --preset windows-x64
#
#  For buils on github, push tags:
#  - git tag v1.0.0
#  - git push origin --tags
cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(GameDetector VERSION 1.0.0)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(ENABLE_INNO_SETUP "Build installer using Inno Setup" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(CURL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl)

set(SETUP_ROOT "${CMAKE_SOURCE_DIR}/setup")
set(SETUP_PLUGINS_DIR "${SETUP_ROOT}/obs-plugins/64bit")
set(SETUP_DATA_DIR "${SETUP_ROOT}/data/obs-plugins/game-detector")

file(MAKE_DIRECTORY "${SETUP_PLUGINS_DIR}")
file(MAKE_DIRECTORY "${SETUP_DATA_DIR}")

set(SETUP_ZIP "${CMAKE_SOURCE_DIR}/setup/GameDetector.zip")

add_custom_command(TARGET GameDetector POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GameDetector>
        "${SETUP_PLUGINS_DIR}/$<TARGET_FILE_NAME:GameDetector>"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "${SETUP_DATA_DIR}"

    COMMAND ${CMAKE_COMMAND} -E tar cfv "${SETUP_ZIP}" --format=zip
        "obs-plugins"
        "data"

    WORKING_DIRECTORY "${SETUP_ROOT}"

    COMMENT "Empacotando arquivos do plugin..."
)

# https://files.jrsoftware.org/is/6/innosetup-6.6.0.exe
# Encontra o compilador do Inno Setup (opcional)
set(INNO_SETUP_COMPILER "C:/Program Files (x86)/Inno Setup 6/ISCC.exe")

if(ENABLE_INNO_SETUP)
        add_custom_command(TARGET GameDetector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Executando Inno Setup..."
        COMMAND "${INNO_SETUP_COMPILER}" "${CMAKE_SOURCE_DIR}/setup.iss"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Criando instalador GameDetector-Setup.exe (opcional)..."
    )
else()
    message(STATUS "Inno Setup desabilitado ou nao encontrado. Ignorando criacao do instalador.")
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Network REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        psapi
        version
    )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    "src/plugin-main.cpp"
    "src/GameDetectorDock.cpp"
    "src/ConfigManager.cpp"
    "src/GameDetector.cpp"
    "src/GameDetectorSettingsDialog.cpp"
    "src/IconProvider.cpp"
    "src/TwitchChatBot.cpp"
    "src/TwitchAuthManager.cpp"
    )

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME "game-detector")
