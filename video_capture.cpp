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
    sleep_ms(10000);
    videos[i].open(url_map[i]);
    if(videos[i].isOpened()){
         std::cout<< "cam: "<< i <<" reconnected"<< std::endl;
         sleep_ms(5000);
	    return;

    }
    else{
          std::cout<< "cam: "<< i <<" reconnected fail"<< std::endl;
          //videos[i].release();
          //reConnect(i);
	    return;
    }
}




int Getbyte( int i,int& width, int& height, int& size, unsigned char*& data) {
    if (!videos[i].isOpened()) {
        std::cout << "cam " << i << " link fail" << std::endl;
        //videos[i].release();
        //reConnect(i);
        return 0;
    }

    cv::Mat frame;
    if (!videos[i].read(frame) || frame.empty()) {
        //std::cout << "Input frame is empty" << std::endl;    
	videos[i].release();
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






