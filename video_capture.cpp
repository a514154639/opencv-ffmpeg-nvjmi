#include "video_capture.h"
#include <iostream>


VideoCapture::VideoCapture(const std::string &filename)
{
    _isOpened = open(filename);
};

VideoCapture::~VideoCapture()
{
    release();
}




bool VideoCapture::open(const std::string &filename){
    AVDictionary *options = NULL;

    //    av_register_all();
    printf("avformat_network_init start\n");
    avformat_network_init();
    // 设置读流相关的配置信息
    av_dict_set(&options, "buffer_size", "1024000", 0); // 设置缓存大小
    av_dict_set(&options, "rtsp_transport", "tcp", 0);  // 以udp的方式打开,
    av_dict_set(&options, "stimeout", "5000000", 0);    // 设置超时断开链接时间，单位us
    av_dict_set(&options, "max_delay", "500000", 0);    // 设置最大时延

    printf("avformat_alloc_context start\n");

    //_fmt_ctx = avformat_alloc_context(); // 用来申请AVFormatContext类型变量并初始化默认参数,申请的空间

    printf("avformat_open_input start\n");

    // 打开网络流或文件流
    if (avformat_open_input(&_fmt_ctx, filename.c_str(), NULL, &options) != 0)
    {
        printf("Couldn't open input stream.\n");
        return 0;
    }

    printf("avformat_find_stream_info start\n");

    // 获取视频文件信息
    if (avformat_find_stream_info(_fmt_ctx, NULL) < 0)
    {
        printf("Couldn't find stream information.\n");
        return 0;
    }

    printf("avformat_find_stream_info end\n");

    // 查找码流中是否有视频流
    unsigned i = 0;
    videoindex = av_find_best_stream(_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    for (i = 0; i < _fmt_ctx->nb_streams; i++)
        if (_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
            break;
        }
    if (videoindex == -1)
    {
        printf("Didn't find a video stream.\n");
        return 0;
    }
    
    // 获取图像宽高
    int srcWidth = _fmt_ctx->streams[videoindex]->codecpar->width;
    int srcHeight = _fmt_ctx->streams[videoindex]->codecpar->height;

    printf("srcWidth is %d, srcHeight is %d\n", srcWidth, srcHeight);

    // _pkt = (AVPacket *)av_malloc(sizeof(AVPacket)); // 申请空间，存放的每一帧数据 （h264、h265）
    // av_new_packet(_pkt, srcWidth * srcHeight);

    _pkt = av_packet_alloc();
    if (!_pkt)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot allocate packet\n");
        return false;
    }
    _fr = av_frame_alloc();
    if (!_fr)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        return false;
    }

    _frBGR = av_frame_alloc();
    if (!_frBGR)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        return false;
    }
    
    decoder = avcodec_find_decoder_by_name("h264_rkmpp");
    _ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(_ctx, _fmt_ctx->streams[videoindex]->codecpar);
    if (avcodec_open2(_ctx, decoder, nullptr) < 0) {
        std::cerr << "Could not open decoder." << std::endl;
        return -1;
    }

    _size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, _ctx->width, _ctx->height,1);
    _out_buffer = (uint8_t *)av_malloc(_size);
    av_image_fill_arrays(_frBGR->data, _frBGR->linesize, _out_buffer, 
                         AV_PIX_FMT_BGR24, _ctx->width, _ctx->height, 1);
    _img_convert_ctx = sws_getContext(_ctx->width, _ctx->height, _ctx->pix_fmt, _ctx->width, _ctx->height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);

    printf("decode init finish.\n");
    
    _isOpened = true;
    return true;

}


bool VideoCapture::read(cv::Mat &image)
{
    if (!_isOpened)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open file\n");
        return false;
    }

    int i = 0;
    int result = 0;
    while (result >= 0)
    {
        result = av_read_frame(_fmt_ctx, _pkt);
        //std::cout << "stream_index: " <<  _pkt->stream_index << " videoindex " << videoindex << std::endl;
        if (result >= 0 && _pkt->stream_index != videoindex)
        {
            av_packet_unref(_pkt);
            continue;
        }

        if (result < 0)
            result = avcodec_send_packet(_ctx, NULL);
        else
        {
            if (_pkt->pts == AV_NOPTS_VALUE)
                _pkt->pts = _pkt->dts = i;
            result = avcodec_send_packet(_ctx, _pkt);
        }
        av_packet_unref(_pkt);

        if (result < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Error submitting a packet for decoding\n");
            return false;
        }
         //std::cout << "result " << result  << std::endl;
        while (result >= 0)
        {
            //std::cout << "start receive frame " << std::endl;
            result = avcodec_receive_frame(_ctx, _fr);
            //std::cout << "finish receive frame " << std::endl;
            if (result == AVERROR_EOF)
            {
                av_log(NULL, AV_LOG_ERROR, "End of file\n");
                return false;
            }
            if (result == AVERROR(EAGAIN))
            {
                result = 0;
                break;
            }
            if (result < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                return false;
            }
            //std::cout << "start convert frame " << std::endl;
            sws_scale(_img_convert_ctx, (const uint8_t *const *)_fr->data, _fr->linesize, 0, _ctx->height, _frBGR->data, _frBGR->linesize); //YUV to RGB
            cv::Mat(_ctx->height, _ctx->width, CV_8UC3, _out_buffer).copyTo(image);
            av_frame_unref(_fr);
            return true;
        }
        i++;
    }
    return true;
}



VideoCapture &VideoCapture::operator>>(cv::Mat &image)
{
    if (!read(image))
        image = cv::Mat();
    return *this;
}

void VideoCapture::release()
{
    av_packet_free(&_pkt);
    av_frame_free(&_fr);
    av_frame_free(&_frBGR);
    avformat_close_input(&_fmt_ctx);
    avcodec_free_context(&_ctx);
    av_freep(&_out_buffer);
}



const int NUM_VIDEOS = 5;
VideoCapture videos[NUM_VIDEOS]; 
std::map<int,std::string> url_map;
std::vector<int> emptyFrameCount(NUM_VIDEOS,0);
std::vector<cv::Mat> cam_map(NUM_VIDEOS);
//cv::Mat frame;

// sleep for ms 
static void sleep_ms(unsigned int secs)
{
    struct timeval tval;
    tval.tv_sec=secs/1000;
    tval.tv_usec=(secs*1000)%1000000;
    select(0,NULL,NULL,NULL,&tval);
}

void Init_uri(int i, const char * uri){       
    std::cout <<uri<< std::endl;
    url_map[i] = uri;
    videos[i].open(uri);
    if(videos[i].isOpened()){
        std::cout<<"Init success"<<std::endl;
    }
    else{
        std::cout<<"Init fail"<< std::endl;
    }
    
    //delete[] res;   
}

bool isConnect(int i){
    if(videos[i].isOpened()){
        return true;
    }
    else{
         std::cout<< "cam: "<< i <<" link fail..."<< std::endl;
        return false;        
    } 
}

void reConnect(int i){
    std::cout<< "cam: "<< i <<" reconnecting..."<< std::endl;
    sleep_ms(5000);
    videos[i].open(url_map[i]);
    if(videos[i].isOpened()){
         std::cout<< "cam: "<< i <<" reconnected"<< std::endl;
         sleep_ms(5000);
    }
    else{
          std::cout<< "cam: "<< i <<" reconnected fail"<< std::endl;
    }
}




int Getbyte( int i,int& width, int& height, int& size, unsigned char*& data) {
    if (!videos[i].isOpened()) {
        std::cout << "cam " << i << " link fail" << std::endl;
        videos[i].release();
        reConnect(i);
        return 0;
    }

    cv::Mat frame;
    if (!videos[i].read(frame) || frame.empty()) {
        //std::cout << "Input frame is empty" << std::endl;    
        return 0;  
    }
    width = frame.cols;
    height = frame.rows;
    size = width*height*3;
    data = frame.data;
    //frame.release();
    return 1;
}

int Getbyte_( int i,int& width, int& height, int& size, cv::Mat** returnframe) {
    if (!videos[i].isOpened()) {
        std::cout << "cam " << i << " link fail" << std::endl;
        return 0;
    }

    cv::Mat frame;
    //std::cout << "start read" << std::endl;  
	videos[i].read(frame);
    if( frame.empty()){
        std::cout << "Input frame is empty" << std::endl;  
            
        videos[i].release();
        return 0;  
    }
    width = frame.cols;
    height = frame.rows;
    size = 0;
   
     *returnframe = new cv::Mat(frame);  // 分配新内存并复制数据
    delete *returnframe;  // It's safe to delete nullptr so no need to check
    //*returnframe = new cv::Mat(frame); // Clone the frame to ensure it's persistent
    //std::cout << &returnframe << std::endl;
    //frame.release();
    return 1;
}
