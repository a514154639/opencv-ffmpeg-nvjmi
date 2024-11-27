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



bool VideoCapture::open(const std::string &filename)
{
    AVDictionary *options = NULL;
    AVStream *st = nullptr;
    av_dict_set(&options, "buffer_size", "1024000", 0); //设置缓存大小,1080p可将值跳到最大
    av_dict_set(&options, "rtsp_transport", "tcp", 0);  //以tcp的方式打开,
    av_dict_set(&options, "stimeout", "5000000", 0);    //设置超时断开链接时间，单位us
    av_dict_set(&options, "max_delay", "500000", 0);    //设置最大时延

     // 分配并初始化一个AVFormatContext
    _fmt_ctx = avformat_alloc_context();
    if (!_fmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate AVFormatContext\n");
        return false;
    }

    // 设置非阻塞模式
    _fmt_ctx->flags |= AVFMT_FLAG_NONBLOCK;

    int result = avformat_open_input(&_fmt_ctx, filename.c_str(), NULL, &options);
    if (result < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        avformat_free_context(_fmt_ctx);
        _fmt_ctx = nullptr;
        return false;
    }

    result = avformat_find_stream_info(_fmt_ctx, NULL);
    if (result < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        avformat_close_input(&_fmt_ctx);
        return false;
    }

    _video_stream = av_find_best_stream(_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (_video_stream < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
        avformat_close_input(&_fmt_ctx);
        return false;
    }

    for (int i = 0; i < _fmt_ctx->nb_streams; i++) {
        av_dump_format(_fmt_ctx, i, filename.c_str(), 0);
    }
    int height = 0;
    int width = 0;

    bool has_264_video = false;
    for (int i = 0; i < _fmt_ctx->nb_streams; i++) {
        st = _fmt_ctx->streams[i];
        switch (st->codecpar->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                height = st->codecpar->height;
                width = st->codecpar->width;
                has_264_video = true;
                break;
            default:
                break;
        }
    }

    //初始化AVPacket
    _pkt = av_packet_alloc();
    if (!_pkt)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot allocate packet\n");
        return false;
    }
    
    jmi_ctx_param.resize_width = width;
    jmi_ctx_param.resize_height = height;

    _size = avpicture_get_size(AV_PIX_FMT_BGR24, width, height);
    _out_buffer = (uint8_t *)av_malloc(_size);

    jmi_ctx_param.coding_type = jmi::NV_VIDEO_CodingH264;
    //jmi_ctx_param.coding_type = jmi::NV_VIDEO_CodingHEVC;
    std::string dec_name = "h264_dec_";
    if (filename.size() >= 5) {
        char fifth_last_char = filename[filename.size() - 5];
        dec_name += fifth_last_char;
    } else {
        // filename长度不足5个字符，可以选择添加默认字符或处理错误
        dec_name += '_'; // 例如使用下划线作为默认字符
    }
    std::cout << dec_name << std::endl;
    jmi_ctx_ = jmi::nvjmi_create_decoder(dec_name.data(), &jmi_ctx_param);
    

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

    int ret = av_read_frame(_fmt_ctx, _pkt);
    if (ret != 0) {
        std::cout << "av_read_frame fail " << ret << std::endl;
        av_packet_unref(_pkt);
        return false;
    }

    auto payload = _pkt->data;
    auto payload_size = _pkt->size;
    nvpacket.payload_size = payload_size;
    nvpacket.payload = payload;
    
    if (payload == nullptr) {
        av_packet_unref(_pkt);
        return false;
    }

    ret = jmi::nvjmi_decoder_put_packet(jmi_ctx_, &nvpacket);
    if(ret == jmi::NVJMI_ERROR_STOP) {
        av_log(NULL, AV_LOG_ERROR, "frameCallback: nvjmi decode error, frame callback EOF!");
        av_packet_unref(_pkt);
        return false;
    }

    jmi::nvFrameMeta nvframe_meta;
    ret = jmi::nvjmi_decoder_get_frame_meta(jmi_ctx_, &nvframe_meta);
    if (ret < 0) {
        av_packet_unref(_pkt);
        return false;
    }

    ret = jmi::nvjmi_decoder_retrieve_frame_data(jmi_ctx_, &nvframe_meta, (void *)_out_buffer);
    if (ret < 0) {
        av_packet_unref(_pkt);
        return false;
    }

    image = cv::Mat(nvframe_meta.height, nvframe_meta.width, CV_8UC3, _out_buffer);
    av_packet_unref(_pkt);		
    return true;
}





bool VideoCapture::read_old(cv::Mat &image)
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

        if (result >= 0 && _pkt->stream_index != _video_stream)
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

        while (result >= 0)
        {
            result = avcodec_receive_frame(_ctx, _fr);
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

            std::chrono::system_clock::time_point start = std::chrono::system_clock::now();    
            //std::cout << "frame type: " << _fr->format << std::endl;
            sws_scale(_img_convert_ctx, (const uint8_t *const *)_fr->data, _fr->linesize, 0, _ctx->height, _frBGR->data, _frBGR->linesize); //YUV to RGB
            //AVFrame2Img_gpu(_fr, _out_buffer);
            auto now = std::chrono::system_clock::now();
            std::chrono::duration<double,  std::milli> elapsed = now - start;
            std::cout << "convert time: " << elapsed.count() << std::endl;

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
    nvjmi_decoder_close(jmi_ctx_);
    nvjmi_decoder_free_context(&jmi_ctx_);
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
         videos[i].release();
         sleep_ms(5000);
         videos[i].open(url_map[i]);
    }
    else{
          std::cout<< "cam: "<< i <<" reconnected fail"<< std::endl;
          videos[i].release();
          reConnect(i);
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


