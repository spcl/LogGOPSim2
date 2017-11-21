cmake_minimum_required(VERSION 2.8)

project(LogGOPSim)

set(CC_FLAGS "-std=c++11")
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(MOD_DIR ${PROJECT_SOURCE_DIR}/simulator/modules)

set(MOD_LOGGOP ${MOD_DIR}/loggop/loggop.cpp)
set(MOD_PORTALS4 ${MOD_DIR}/portals4/p4.cpp)
set(MOD_PORTALS4_SMP ${MOD_DIR}/portals4_smp/p4smp.cpp ${MOD_DIR}/portals4_smp/gem5/GEM5Mod.cpp)
set(MOD_PACKET_NET ${MOD_DIR}/packet_net/NetMod.cpp)
set(MOD_DMA ${MOD_DIR}/dma/dma.cpp)

set(MOD_SOURCES ${MOD_LOGGOP} ${MOD_PORTALS4} ${MOD_PORTALS4_SMP} ${MOD_PACKET_NET} ${MOD_DMA})

add_definitions(${CC_FLAGS})

add_executable(LogGOPSim ${PROJECT_SOURCE_DIR}/simulator/sim.cpp ${PROJECT_SOURCE_DIR}/simulator/main.cpp ${PROJECT_SOURCE_DIR}/simulator/cmdline.c ${MOD_SOURCES})
add_custom_target(cmdline COMMAND gengetopt --output-dir=${PROJECT_SOURCE_DIR}/simulator < ${PROJECT_SOURCE_DIR}/simulator/cmdline.ggo)
add_dependencies(LogGOPSim cmdline)
