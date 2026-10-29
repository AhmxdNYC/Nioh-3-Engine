cmake_minimum_required(VERSION 3.21)
project(Nioh3EngineFix LANGUAGES CXX ASM_MASM)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Nioh3EngineFix is x64 only")
endif()

# patches_gen.h is generated from patches.json (single source of truth); regenerate when Python is available
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/patches_gen.h
        COMMAND Python3::Interpreter -I ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_header.py
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/patches.json ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_header.py
        COMMENT "Generating patches_gen.h from patches.json")
endif()

add_library(Nioh3EngineFix SHARED src/dllmain.cpp src/osd.cpp src/hitchlog.cpp src/dxgidiag.cpp src/menu.cpp src/gpuopt.cpp src/gimerge.cpp src/skipfx.cpp src/livectl.cpp src/gpuprof.cpp src/memstats.cpp src/upmirror.cpp src/tilestats.cpp src/loadmem.cpp src/loadstats.cpp src/fnprobe.cpp src/padlat.cpp src/tour.cpp src/vpad.cpp src/sp.cpp src/psoswap.cpp src/psojournal.cpp src/autocont.cpp src/bootnotice.cpp src/crashreport.cpp src/updatecheck.cpp src/readprobe.cpp src/readfix.cpp src/prefetch.cpp src/loadonly.cpp src/drawprep.cpp src/drawprep.asm src/jt.cpp src/jt.asm src/pmode.cpp src/pmode.asm src/asyncslots.cpp src/asyncslots.asm src/timerres.cpp src/thrsched.cpp src/psomemo.cpp src/bkstats.cpp src/rnt.cpp src/frameoverlap.cpp src/replayprof.cpp src/replayprof.asm src/cbvupload.cpp src/cbvstage.cpp src/samelook.cpp src/samelook.asm src/extraperf.cpp src/extraperf.asm src/mlprobe.cpp src/mpccheck.cpp src/mpcskip.asm src/caves.asm src/gpuopt_thunks.asm src/patches_gen.h)
set_target_properties(Nioh3EngineFix PROPERTIES
    SUFFIX ".asi"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    MSVC_RUNTIME_LIBRARY "MultiThreaded")          # static CRT (/MT): no VC++ redistributable needed
target_compile_definitions(Nioh3EngineFix PRIVATE UNICODE _UNICODE NDEBUG)
target_compile_options(Nioh3EngineFix PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/O2 /GS /Gy /Gw /EHsc /W4 /permissive->)
target_link_options(Nioh3EngineFix PRIVATE /OPT:REF /OPT:ICF /DEBUG:FULL)
target_link_libraries(Nioh3EngineFix PRIVATE kernel32 user32 bcrypt psapi shell32 winhttp)
