###########################################################################
#
# Configuration
#
###########################################################################

# OCLTOYS_CUSTOM_CONFIG
# E:/projects/ocltoys-32bit/ocltoys/cmake/SpecializedConfig/Config_Dade-win32.cmake
# 
# Boost:
#  bjam --toolset=msvc --stagedir=stage stage -j 8
#
# To disable OpenMP support (VCOMP.lib error) in Visula Studio 2010 go
# properties => C/C++ => languages and disable OpenMP

MESSAGE(STATUS "Using Dade's Win32 Configuration settings")

set(CMAKE_BUILD_TYPE "Release")

set(BOOST_SEARCH_PATH         "E:/projects/ocltoys-32bit/boost_1_48_0")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/stage/boost/lib")

set(OPENCL_SEARCH_PATH        "C:/Program Files (x86)/AMD APP")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86")

# For GLUT with sources
set(GLUT_SEARCH_PATH          "E:/projects/ocltoys-32bit/freeglut-2.8.0")
set(GLUT_LIBRARYDIR           "${GLUT_SEARCH_PATH}/lib/x86")
ADD_DEFINITIONS(-DFREEGLUT_STATIC)
# For GLUT with binary-only
#set(GLUT_SEARCH_PATH          "E:/projects/ocltoys-32bit/freeglut-bin")
#set(GLUT_LIBRARYDIR           "${GLUT_SEARCH_PATH}/lib")
