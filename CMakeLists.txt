cmake_minimum_required (VERSION 3.19)

if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    #set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")

    # https://cmake.org/cmake/help/latest/prop_tgt/MSVC_DEBUG_INFORMATION_FORMAT.html#prop_tgt:MSVC_DEBUG_INFORMATION_FORMAT
    # set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("VoxelGame")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(GameSources

)

# Dependencies
set(SDL3_DIR "vendor/SDL-3.2.20")
set(IMGUI_DIR "vendor/imgui-1.92.2b-docking")
set(GLM_DIR "vendor/GLM-1.0.1")

set(ImGuiSources
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)

# Actual Executables
add_executable(GameClient "src/client_main.cpp" ${GameSources} ${ImGuiSources})
add_executable(GameServer "src/server_main.cpp" ${GameSources})
add_executable(GameTests "src/test.cpp" ${GameSources} ${ImGuiSources})

# Include / Link
add_subdirectory(${SDL3_DIR})
add_subdirectory(${GLM_DIR})

target_include_directories(GameClient PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_include_directories(GameTests PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)

target_link_libraries(GameClient PRIVATE SDL3::SDL3)
target_link_libraries(GameTests PRIVATE SDL3::SDL3)

target_link_libraries(GameClient PRIVATE glm::glm)
target_link_libraries(GameServer PRIVATE glm::glm)
target_link_libraries(GameTests PRIVATE glm::glm)

# Ensure all warnings are caught
if(MSVC)
    add_compile_options(/W4 /WX dbghelp /EHsc /Zi /fsanitize=address)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -fsanitize=address)
    set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(Game PRIVATE SYNC_LIB_NO_TESTS NO_TESTS)
endif()

# SIMD
include(CheckCXXCompilerFlag)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    set (X86 TRUE)
else ()
    set (X86 FALSE)
endif ()

if(X86)
    # SSE2
    cmake_host_system_information(RESULT HAS_SSE2 QUERY HAS_SSE2)
    if(HAS_SSE2)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            message(STATUS "Game SSE2 GNU/Clang")
            set(CMAKE_CXX_FLAGS "-msse2")
            add_compile_options(-msse2)
        elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
            message(STATUS "Game SSE2 MSVC")
            set(CMAKE_CXX_FLAGS "/arch:SSE2")
            add_compile_options(/arch:SSE2)
            # For some reason __SSE2__ macro doesn't seem to work? Not sure why?
        endif()
    endif()

    # AVX2
    if(MSVC)
        message(STATUS "Game AVX2 MSVC")
        set(CMAKE_CXX_FLAGS "/arch:AVX2")
        add_compile_options(/arch:AVX2)
    else()
        message(STATUS "Game AVX2 GNU/Clang")
        set(CMAKE_CXX_FLAGS "-mavx2")
        add_compile_options(-mavx2)
    endif()
endif()

enable_testing()
add_test(GameTests GameTests)
