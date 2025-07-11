#include "video_capture.h"
#include "nv12tobgr.h"
#include <iostream>

VideoCapture::VideoCapture(const std::string &filename)
{
    _isOpened = open(filename);
};

VideoCapture::~VideoCapture()
{
    release();
}

std::string getcurrent_time(){
    auto now = std::chrono::system_clock::now();
    // 转换为time_t格式
    auto now_c = std::chrono::system_clock::to_time_t(now);
    // 转换为tm结构
    std::tm now_tm = *std::localtime(&now_c);

    // 获取当前毫秒数（不是从当前秒开始的毫秒数）
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    // 构建文件名，使用setw和setfill来确保格式正确（如单数字前补0）
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%m-%d-%H-%M-%S-") // 格式化月、日、时、分、秒
    << std::setw(3) << std::setfill('0') << milliseconds; // 毫秒数格式化为三位数
    std::string filename = ss.str();
    return filename;
}


// bool VideoCapture::open(const std::string &filename)
// {
//     AVDictionary *options = NULL;
//     av_dict_set(&options, "buffer_size", "1024000", 0); //设置缓存大小,1080p可将值跳到最大
//     av_dict_set(&options, "rtsp_transport", "tcp", 0);  //以tcp的方式打开,
//     av_dict_set(&options, "stimeout", "5000000", 0);    //设置超时断开链接时间，单位us
//     av_dict_set(&options, "max_delay", "500000", 0);    //设置最大时延
//     int result = avformat_open_input(&_fmt_ctx, filename.c_str(), NULL, &options);
//     if (result < 0)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
//         return false;
//     }

//     result = avformat_find_stream_info(_fmt_ctx, NULL);
//     if (result < 0)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
//         return false;
//     }

//     _video_stream = av_find_best_stream(_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
//     if (_video_stream < 0)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
//         return false;
//     }

//     _origin_par = _fmt_ctx->streams[_video_stream]->codecpar;
//     //_codec = avcodec_find_decoder(_origin_par->codec_id);hevc_nvv4l2dec
//     _codec = avcodec_find_decoder_by_name("h264_nvmpi");
//     if (!_codec)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
//         return false;
//     }

//     _ctx = avcodec_alloc_context3(_codec);
//     if (!_ctx)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
//         return false;
//     }

//     result = avcodec_parameters_to_context(_ctx, _origin_par);
//     //_ctx->pix_fmt =  AV_PIX_FMT_NV12; /*AV_PIX_FMT_NV12*/
//     if (result)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
//         return false;
//     }

//     result = avcodec_open2(_ctx, _codec, NULL);
//     if (result < 0)
//     {
//         av_log(_ctx, AV_LOG_ERROR, "Can't open decoder\n");
//         return false;
//     }

//     _fr = av_frame_alloc();
//     if (!_fr)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
//         return false;
//     }

//     _frBGR = av_frame_alloc();
//     if (!_frBGR)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
//         return false;
//     }

//     _size = avpicture_get_size(AV_PIX_FMT_BGR24, _ctx->width, _ctx->height);
//     _out_buffer = (uint8_t *)av_malloc(_size);
//     avpicture_fill((AVPicture *)_frBGR, _out_buffer, AV_PIX_FMT_BGR24, _ctx->width, _ctx->height);
//     //out_data = new uchar[_ctx->width * _ctx->height * 3]; 
//     _img_convert_ctx = sws_getContext(_ctx->width, _ctx->height, _ctx->pix_fmt, _ctx->width, _ctx->height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);

//     _pkt = av_packet_alloc();
//     if (!_pkt)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Cannot allocate packet\n");
//         return false;
//     }
//     _isOpened = true;
//     return true;
// }


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
        _isOpened = false;
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
        _isOpened = false;
        return false;
    }

    result = avformat_find_stream_info(_fmt_ctx, NULL);
    if (result < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        avformat_close_input(&_fmt_ctx);
        _isOpened = false;
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

    time_base = st->time_base;
    //starttime = std::chrono::system_clock::now();
    //auto start = getcurrent_time();
    //std::cout << "starttime: " << start << std::endl;
	std::cout << "时间基格式：" << time_base.num << "/" << time_base.den 
          << " (约等于 " << av_q2d(time_base) << " 秒/单位)";
	
    //初始化AVPacket
    _pkt = av_packet_alloc();
    if (!_pkt)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot allocate packet\n");
        _isOpened = false;
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
    
    av_dict_free(&options);

    _isOpened = true;
    return true;
}




bool VideoCapture::read(cv::Mat &image) {
    if (!_isOpened) return false;
    
    while (av_read_frame(_fmt_ctx, _pkt) >= 0) {
        // 基础校验
        if (_pkt->size <= 0 || _pkt->data == nullptr) {
            av_packet_unref(_pkt);
            continue;
        }

        // 投递数据包
        nvpacket.payload = _pkt->data;
        nvpacket.payload_size = _pkt->size;
        if (jmi::nvjmi_decoder_put_packet(jmi_ctx_, &nvpacket) != 0) {
            av_packet_unref(_pkt);
            continue;
        }

        // 提取帧数据
        int frame_ret;
        while ((frame_ret = jmi::nvjmi_decoder_get_frame_meta(jmi_ctx_, &nvframe_meta)) >= 0) {
            qframe_size = frame_ret;
            if (jmi::nvjmi_decoder_retrieve_frame_data(jmi_ctx_, &nvframe_meta, _out_buffer) < 0) 
                break;
            
            image = cv::Mat(nvframe_meta.height, nvframe_meta.width, CV_8UC3, _out_buffer);
            av_packet_unref(_pkt);
            return true;
        }
        av_packet_unref(_pkt);
    }
    return false;
}





bool VideoCapture::read_bac(cv::Mat &image)
{
    if (!_isOpened)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open file\n");
        return false;
    }
    int ret = 0;
    while (true) {
        //auto before_read_time = getcurrent_time();
        //std::cout << "before_read_time: " << before_read_time << std::endl;
        ret = av_read_frame(_fmt_ctx, _pkt);
        if (ret != 0) {
            std::cout << "av_read_frame fail " << ret << std::endl;
            av_packet_unref(_pkt); // 移动到这里
            return false; // 直接返回
        } else {
            auto payload = _pkt->data;
            auto payload_size = _pkt->size;
            //auto pts = _pkt->pts;
            nvpacket.payload_size = payload_size;
            nvpacket.payload = payload;
            //nvpacket.pts = pts;
            
            // if (_pkt->pts != AV_NOPTS_VALUE) {
            //     double pts_time_seconds = _pkt->pts * av_q2d(time_base);
            //     std::cout << "pts_time_seconds: " << pts_time_seconds*1000 << std::endl;

            // }
            //auto pkt_time = getcurrent_time();
            //std::cout << "pkt_time: " << pkt_time << std::endl;
            if (payload != nullptr) {
                ret = jmi::nvjmi_decoder_put_packet(jmi_ctx_, &nvpacket);
                if(ret == jmi::NVJMI_ERROR_STOP) {
                    av_log(NULL, AV_LOG_ERROR, "frameCallback: nvjmi decode error, frame callback EOF!");
                    av_packet_unref(_pkt); // 确保在退出前释放
                    return false;
                }
                while (ret >= 0) {
                    ret = jmi::nvjmi_decoder_get_frame_meta(jmi_ctx_, &nvframe_meta);
                    if (ret < 0)
                    {
                        av_packet_unref(_pkt);
                        return false;
                    }
                    else {
                        qframe_size = ret;
                        //std::cout << qframe_size << std::endl;
                    }

                    ret = jmi::nvjmi_decoder_retrieve_frame_data(jmi_ctx_, &nvframe_meta, (void *)_out_buffer);

                    //auto nvframe_time = getcurrent_time();
                    //std::cout << "nvframe_time: " << nvframe_time << std::endl;
                    image = cv::Mat(nvframe_meta.height, nvframe_meta.width, CV_8UC3, _out_buffer);					
                    av_packet_unref(_pkt);		
                    return true;
                }
            }
        }       
    }
    return false; // 确保函数有一个明确的退出点
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
            //std::cout << "convert time: " << elapsed.count() << std::endl;

            //cv::Mat(_ctx->height, _ctx->width, CV_8UC3, _out_buffer).copyTo(image);
            image = cv::Mat(_ctx->height, _ctx->width, CV_8UC3, _out_buffer);
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


// void VideoCapture::reconnect(int i) {
//     // 关闭并释放当前连接资源
//     if (_isOpened) {
//         avformat_close_input(&_fmt_ctx);
//         if (_pkt) {
//             av_packet_free(&_pkt);
//             _pkt = nullptr;
//         }
//         if (_out_buffer) {
//             av_free(_out_buffer);
//             _out_buffer = nullptr;
//         }
//         if (jmi_ctx_) {
//             jmi::nvjmi_destroy_decoder(jmi_ctx_);
//             jmi_ctx_ = nullptr;
//         }
//         _isOpened = false;
//     }
    
//     std::cout << "Attempting to reconnect... Attempt " << std::endl;
//     if (open(url_map[i])) { // 假设_filename是类中存储的文件名
//         std::cout << "Reconnection successful." << std::endl;
//         return;
//     } else {
//         std::cout << "Reconnection failed. Retrying..." << std::endl;
//         // 释放由于失败的连接尝试可能分配的资源
//         avformat_close_input(&_fmt_ctx);
//         if (_pkt) {
//             av_packet_free(&_pkt);
//             _pkt = nullptr;
//         }
//         if (_out_buffer) {
//             av_free(_out_buffer);
//             _out_buffer = nullptr;
//         }
//         if (jmi_ctx_) {
//             jmi::nvjmi_destroy_decoder(jmi_ctx_);
//             jmi_ctx_ = nullptr;
//         }
//         // 等待一段时间再重试
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }

//     std::cerr << "Failed to reconnect after " << retries << " attempts." << std::endl;
// }



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
    //videos[i].release();
    sleep_ms(10000);
    videos[i].open(url_map[i]);
    if(videos[i].isOpened()){
         std::cout<< "cam: "<< i <<" reconnected"<< std::endl;
         //videos[i].release();
         sleep_ms(5000);
         //videos[i].open(url_map[i]);
    }
    else{
          std::cout<< "cam: "<< i <<" reconnected fail"<< std::endl;
          //videos[i].release();
          //reConnect(i);
    }
}


// int Getbyte(int i, cv::Mat **returnframe)
// {
//   cv::Mat frame;
//   if (videos[i].isOpened()) {
//       videos[i].read(frame);
//       if (frame.empty()) {
//           std::cout << "Input frame is empty" << std::endl;    
//           return 0;  
//       } 
//       else {
//         if (*returnframe != nullptr) {
//             delete *returnframe;  // 释放之前分配的内存
//         }
//            *returnframe =  new cv::Mat(frame);
//             return 1;  
//       }
//   } else {
//       std::cout << "cam " << i << " link fail" <<  std::endl;
//       videos[i].release();
//       reConnect(i);
//       return 0;

//   }
// }

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
    //size = videos[i].qframe_size + 25;
    size = 30;
    data = frame.data;
    //std::cout << "qframe size: " << size << std::endl;  
    //frame.release();
    return 1;
}

// int Getbyte(int i,int& width, int& height, int& size, unsigned char*& data) {
//     if (!videos[i].isOpened()) {
//         std::cout << "cam " << i << " link fail" << std::endl;
//         videos[i].release();
//         reConnect(i);
//         return 0;
//     }
//     cv::Mat frame;
//     if (!videos[i].read(frame)) {
//         if (frame.empty()) {
//           //std::cout << "Input frame is empty, restarting video" << std::endl;
//           // 重新打开视频，从头开始播放
//           //videos[i].set(cv::CAP_PROP_POS_FRAMES, 0);
//           //std::cout << "Input frame is empty" << std::endl;
//           if (++emptyFrameCount[i] >= 3) {
//             std::cout << "5 consecutive empty frames detected. Reconnecting to cam" << i <<  std::endl;
//             videos[i].release();
//             // videos[i].open(url_map[i]);
//             // videos[i].read(frame);
//             emptyFrameCount[i] = 0; 
//             reConnect(i);
            
//           }
          
//           if (frame.empty()) return 0;  // 如果重置后仍然空，则返回0
//         }
//     }
//     width = frame.cols;
//     height = frame.rows;
//     size = videos[i].qframe_size;
//     data = frame.data;

//     return 1;
// }



// int Getbyte( int i, cv::Mat **returnframe) {
//     if (!videos[i].isOpened()) {
//         std::cout << "cam " << i << " link fail" << std::endl;
//         videos[i].release();
//         reConnect(i);
//         return 0;
//     }

//     cv::Mat frame;
//     if (!videos[i].read(frame) || frame.empty()) {
//         std::cout << "Input frame is empty" << std::endl;    
//         return 0;  
//     }
//     if (*returnframe != nullptr) {
//         delete *returnframe;  // 释放之前分配的内存
//     }
//     *returnframe = new cv::Mat(frame);  // 分配新内存并复制数据
//     return 1;
// }


