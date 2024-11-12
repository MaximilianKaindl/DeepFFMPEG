# CMakeList.txt : CMake project for DeepFFMPEG, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("DeepFFMPEG")

find_package(fmt CONFIG REQUIRED)

# Add source to this project's executable.
add_executable (DeepFFMPEG "DeepFFMPEG.cpp" "DeepFFMPEG.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET DeepFFMPEG PROPERTY CXX_STANDARD 20)
endif()

# Define cache variables with default values if not set
# Get the FFMPEG_BUILD from environment if not already set in cache
set(FFMPEG_BUILD $ENV{FFMPEG_BUILD})

# Normalize paths for Ninja
file(TO_CMAKE_PATH "${FFMPEG_BUILD}" FFMPEG_BUILD)
set(FFMPEG_LIBRARY_PATH "${FFMPEG_BUILD}/lib")
set(FFMPEG_BIN_PATH "${FFMPEG_BUILD}/bin")

# Include directories
include_directories(${FFMPEG_BUILD}/include)

# Add library directories
link_directories(${FFMPEG_LIBRARY_PATH})

# Set the correct library suffix for Windows and configure DLL copying
if(WIN32)
    set(LIB_SUFFIX ".lib")
    
    # List of required FFmpeg DLLs with versions
    set(FFMPEG_DLLS
        avformat-61
        avcodec-61
        avdevice-61
        avfilter-10
        avutil-59
        swresample-5
        swscale-8
        postproc-58
    )
    
    foreach(DLL ${FFMPEG_DLLS})
        add_custom_command(
            TARGET DeepFFMPEG 
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FFMPEG_BIN_PATH}/${DLL}.dll"
                $<TARGET_FILE_DIR:DeepFFMPEG>
            COMMENT "Copying ${DLL}.dll to output directory"
        )
    endforeach()

    # Print DLL copy status for debugging
    foreach(DLL ${FFMPEG_DLLS})
        if(EXISTS "${FFMPEG_BIN_PATH}/${DLL}.dll")
            message(STATUS "Found DLL: ${FFMPEG_BIN_PATH}/${DLL}.dll")
        else()
            message(WARNING "Missing DLL: ${FFMPEG_BIN_PATH}/${DLL}.dll")
        endif()
    endforeach()
else()
    set(LIB_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

# Define target libraries with Windows-specific library names
target_link_libraries(DeepFFMPEG
    PRIVATE
        # FFmpeg libraries with explicit .lib extension for Windows
        ${FFMPEG_LIBRARY_PATH}/avformat${LIB_SUFFIX}
        ${FFMPEG_LIBRARY_PATH}/avcodec${LIB_SUFFIX}
        ${FFMPEG_LIBRARY_PATH}/avutil${LIB_SUFFIX}
        ${FFMPEG_LIBRARY_PATH}/avdevice${LIB_SUFFIX}
        ${FFMPEG_LIBRARY_PATH}/swscale${LIB_SUFFIX}
        ${FFMPEG_LIBRARY_PATH}/swresample${LIB_SUFFIX}
        
        # Windows-specific system libraries
        $<$<PLATFORM_ID:Windows>:wsock32>
        $<$<PLATFORM_ID:Windows>:ws2_32>
        $<$<PLATFORM_ID:Windows>:Secur32>
        $<$<PLATFORM_ID:Windows>:BCrypt>
)

# Optional: Print library paths for debugging
message(STATUS "FFmpeg directory: ${FFMPEG_BUILD}")
message(STATUS "FFmpeg library path: ${FFMPEG_LIBRARY_PATH}")
message(STATUS "FFmpeg binary path: ${FFMPEG_BIN_PATH}")

foreach(lib avformat avcodec avutil avdevice swscale swresample)
    if(EXISTS "${FFMPEG_LIBRARY_PATH}/${lib}${LIB_SUFFIX}")
        message(STATUS "Found library: ${FFMPEG_LIBRARY_PATH}/${lib}${LIB_SUFFIX}")
    else()
        message(WARNING "Missing library: ${FFMPEG_LIBRARY_PATH}/${lib}${LIB_SUFFIX}")
    endif()
endforeach()

target_link_libraries(DeepFFMPEG PRIVATE fmt::fmt)