cmake_minimum_required(VERSION 3.16)
project(jarvis_son VERSION 1.0.0 LANGUAGES CXX)

# Jarvis-son V1 — orquestador por turnos en C++17.
# Compila igual en Termux (clang, Android arm64) y en Linux (CI).

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de compilación" FORCE)
endif()

option(JARVIS_PRUEBAS "Compilar las pruebas unitarias e integración" ON)
option(JARVIS_WERROR "Tratar advertencias como errores (CI)" OFF)

# Todas las fuentes de src/ menos main.cpp forman la biblioteca del núcleo.
file(GLOB JARVIS_FUENTES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM JARVIS_FUENTES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(jarvis_nucleo STATIC ${JARVIS_FUENTES})
target_include_directories(jarvis_nucleo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(jarvis_nucleo PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
if(JARVIS_WERROR)
  target_compile_options(jarvis_nucleo PRIVATE -Werror)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
  add_executable(jarvis src/main.cpp)
  target_link_libraries(jarvis PRIVATE jarvis_nucleo)
  target_compile_options(jarvis PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
  if(JARVIS_WERROR)
    target_compile_options(jarvis PRIVATE -Werror)
  endif()
endif()

if(JARVIS_PRUEBAS)
  enable_testing()
  file(GLOB JARVIS_PRUEBAS_FUENTES CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp")
  add_executable(pruebas_unitarias ${JARVIS_PRUEBAS_FUENTES})
  target_link_libraries(pruebas_unitarias PRIVATE jarvis_nucleo)
  target_include_directories(pruebas_unitarias PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit")
  target_compile_definitions(pruebas_unitarias
                             PRIVATE JARVIS_DIR_FUENTE="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME unitarias COMMAND pruebas_unitarias)

  if(TARGET jarvis AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/integracion/prueba_flujo.sh")
    add_test(NAME integracion_flujo
             COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/integracion/prueba_flujo.sh"
                     $<TARGET_FILE:jarvis>)
  endif()
endif()
