cmake_minimum_required(VERSION 3.10)
project(RTSPDecodeExample)

set(CMAKE_CXX_STANDARD 14)


# set(ffmpeg_libs_DIR /usr/lib)                         ## ffmpeg
# set(ffmpeg_headers_DIR /usr/include) 

set(rga_libs_DIR /usr/lib/librga.so)                         ## rga
set(rga_headers_DIR /usr/include/rga) 

# link_directories(
#     mpp_libs
# )

find_package(OpenCV REQUIRED)


# include_directories(
#     ${OpenCV_INCLUDE_DIRS}
#     ${rga_headers_DIR}
#     ${CMAKE_CURRENT_LIST_DIR}/mpp-develop/inc
#     ${CMAKE_CURRENT_LIST_DIR}/mpp-develop/osal/inc
#     ${CMAKE_CURRENT_LIST_DIR}/mpp-develop/utils
#     ${CMAKE_CURRENT_LIST_DIR}/mpp-develop/mpp/inc
#     ${CMAKE_CURRENT_LIST_DIR}/mpp-develop/mpp/base/inc
# )

find_package(PkgConfig REQUIRED)
pkg_search_module(AVCODEC REQUIRED libavcodec)
pkg_search_module(AVFORMAT REQUIRED libavformat)
pkg_search_module(AVUTIL REQUIRED libavutil)
pkg_search_module(SWSCALE REQUIRED libswscale)
pkg_search_module(AVFILTER REQUIRED libavfilter)

include_directories(${AVCODEC_INCLUDE_DIRS})
link_directories(${AVCODEC_LIBRARY_DIRS})


add_library(RtspClientLib SHARED  video_capture.cpp)

add_executable(RTSPDecode
    #${CMAKE_CURRENT_LIST_DIR}/media/mpi_dec.cpp
    video_capture.h
    video_capture.cpp
    main.cpp
)

#target_include_directories(RTSPDecode PRIVATE ${ffmpeg_headers_DIR} )

target_link_libraries(RTSPDecode      
    ${OpenCV_LIBS}
    #${rga_libs_DIR}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
     ${AVFILTER_LIBRARIES}
    #rockchip_mpp
    # utils # 在mpp_libs下，MPP工具库
    # mk_api # 在mpp_libs下，ZLMediaKit API库
    -pthread                                       
)

target_link_libraries(RtspClientLib     
    ${OpenCV_LIBS}
    #${rga_libs_DIR}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
     ${AVFILTER_LIBRARIES}
    #rockchip_mpp
    # utils # 在mpp_libs下，MPP工具库
    # mk_api # 在mpp_libs下，ZLMediaKit API库
    -pthread                                       
)
