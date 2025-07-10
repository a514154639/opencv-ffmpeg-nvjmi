#pragma once
#include <string>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <unistd.h>
#include <map>
//#include "media/mpi_dec.h"

#ifdef __cplusplus
extern "C"
{
#endif
/*Include ffmpeg header file*/
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>

#ifdef __cplusplus
}
#endif



class VideoCapture
{
public:
    explicit VideoCapture(const std::string &filename);
    ~VideoCapture();
    bool isOpened() { return _isOpened; };
    VideoCapture &operator>>(cv::Mat &image);
    VideoCapture(){};
    //bool open(const std::string &filename);
    bool open(const std::string &filename);
    bool read(cv::Mat &image);
    void release();
    

//private:
    
    

private:
    
    mutable bool _isOpened{false};
    const AVCodec *decoder{NULL};
    AVCodecContext *_ctx{NULL};
    AVCodecParameters *_origin_par{NULL};
    AVFrame *_fr{NULL};
    AVFrame *_frBGR{NULL};
    AVPacket *_pkt{NULL};
    AVFormatContext *_fmt_ctx{NULL};
    struct SwsContext *_img_convert_ctx{NULL};
    uint8_t *_out_buffer{NULL};
    uchar * out_data{NULL};
    int _size{-1};
    int videoindex{-1};

};

extern "C" void Init_uri(int i, const char * uri);
extern "C" bool isConnect(int i);
//extern "C" void Getbyte(unsigned char *image,int i);
extern "C" void reConnect(int i);
extern "C" int Getbyte( int i,int& width, int& height, int& size, unsigned char*& data);
extern "C" int Getbyte_( int i,int& width, int& height, int& size, cv::Mat** returnframe);
//extern "C" int Getbyte( int i, cv::Mat **returnframe);


