cmake_minimum_required(VERSION 3.10)
project(RTSPDecodeExample)

set(CMAKE_CXX_STANDARD 14)

set(nvmpi_libs_DIR /usr/local/lib)               ## nvmpi
set(nvmpi_headers_DIR /usr/local/include)
add_library(nvmpi SHARED IMPORTED )
set_target_properties(nvmpi PROPERTIES IMPORTED_LOCATION ${nvmpi_libs_DIR}/libnvmpi.so )

set(ffmpeg_libs_DIR /usr/local/lib)                         ## ffmpeg
set(ffmpeg_headers_DIR /usr/local/include) 


set(nvjmi_libs_DIR /usr/local/lib)               ## nvjmi
set(nvjmi_headers_DIR /usr/local/include)
add_library(nvjmi SHARED IMPORTED )
set_target_properties(nvjmi PROPERTIES IMPORTED_LOCATION ${nvjmi_libs_DIR}/libnvjmi.so )

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

find_package(CUDA REQUIRED)
# cuda
include_directories(/usr/local/cuda-10.2/include)
link_directories(/usr/local/cuda-10.2/lib64)

enable_language(CUDA)


add_library( avcodec SHARED IMPORTED )
 
set_target_properties( avcodec PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavcodec.so )
 
add_library( avfilter SHARED IMPORTED )
set_target_properties( avfilter PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavfilter.so )
 
add_library( swresample SHARED IMPORTED )
set_target_properties( swresample PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libswresample.so )
 
add_library( swscale SHARED IMPORTED )
set_target_properties( swscale PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libswscale.so )
 
add_library( avformat SHARED IMPORTED )
set_target_properties( avformat PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavformat.so )
 
add_library( avutil SHARED IMPORTED )
set_target_properties( avutil PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavutil.so ) 


cuda_add_library(RtspClientLib SHARED  video_capture.cpp)

cuda_add_executable(RTSPDecode
    video_capture.h
    video_capture.cpp
    main.cpp
    ${CUDA_SOURCES}
)

target_include_directories(RTSPDecode PRIVATE ${nvmpi_headers_DIR} ${nvjmi_headers_DIR} ${ffmpeg_headers_DIR} )

target_link_libraries(RTSPDecode      
    ${OpenCV_LIBS}
    ${CUDA_LIBRARIES} nppicc nppc  nppisu nppig cudart 
    -pthread                                       
    avformat avcodec nvjmi  nvmpi  avfilter  avutil avdevice swscale swresample
)

target_link_libraries(RtspClientLib     
    ${OpenCV_LIBS}
    ${CUDA_LIBRARIES} nppicc nppc  nppisu nppig cudart 
    -pthread                                       
    avformat avcodec nvjmi  nvmpi  avfilter  avutil avdevice swscale swresample 
)
