# DeepFFMPEG

## Installation Guide

This Project uses CMake and Ninja to build. VCPKG is used as the Packagemanager.
Pytorch and FFMPEG are prebuilt and have to their paths have to be included via the "FFMPEG_BUILD" "TORCH_BUILD" Evnironment Variables, which are set in CMakeUserPresets.json.