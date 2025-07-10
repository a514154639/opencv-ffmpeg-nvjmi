# opencv-ffmpeg-nvjmi-mpp
ffmpeg调用jetson/rk3588 mpp硬解码，重写了VideoCapture的open与read函数，支持h264/h265

参考仓库 https://github.com/fan-chao/nvjmi
需要先编译nvjmi的动态库文件

仓库中的so基于jepack 4.6.2

rk3588需要编译mpp与ffmpeg，参考仓库https://github.com/nyanmisaka/ffmpeg-rockchip
