#include <iostream>
#include <vector>
#include <chrono>
#include <time.h>
#include <cmath>
#include <string>
#include <thread>
#include <opencv2/opencv.hpp>
#include "video_capture.h"

int test_VideoCapture();

typedef std::chrono::high_resolution_clock clock_;
typedef std::chrono::duration<float, std::milli> duration;


static void sleep_ms(unsigned int secs)
{
    struct timeval tval;
    tval.tv_sec=secs/1000;
    tval.tv_usec=(secs*1000)%1000000;
    select(0,NULL,NULL,NULL,&tval);
}

void test_cam_frame(int camIndex) {
    int frameCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();  
    cv::Mat* frame;
    while (true) {       
        if (isConnect(camIndex)) {
            int width;
            int height;
            int size;
            unsigned char* data;       
            auto decstartTime = std::chrono::high_resolution_clock::now();   
            int status = Getbyte(camIndex,width,height,size,data);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double,std::milli> dec_time = end - decstartTime;   
            std::cout << "dec_time: " << dec_time.count() << std::endl;     
                       
            if (status == 1){             
                cv::Mat img(height,width, CV_8UC3, data);   
                                             
                //cv::imwrite("res.jpg", img);
                 frameCount++;
            }

            //Getbyte(camIndex, &frame);
            //cv::imwrite("res.jpg", *frame);

        } else {
            reConnect(camIndex);
        }

       
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = currentTime - startTime;

        if (elapsed.count() >= 5.0) { // 每5秒钟
            double fps = frameCount / elapsed.count();
            std::cout << "Camera " << camIndex << " FPS: " << fps << std::endl;

            // 重置计时器和帧计数
            startTime = std::chrono::high_resolution_clock::now();
            frameCount = 0;
        }
    }
    delete frame;
}


int test_VideoCapture()
{
    VideoCapture cap("rtsp://admin:admin@192.168.1.103:554");
    if (!cap.isOpened())
    {
        std::cout << "Open capture failed";
        return 0;
    }
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    int frame_count = 0;
    int count = 1000;
    while (true)
    {
        cv::Mat frame;
        cap >> frame;
        frame_count++;
        if (frame.empty())
            break;
        //std::cout << frame.cols << "  " << frame.rows << std::endl;
        
        cv::imwrite("res.jpg", frame);
        sleep_ms(500);
        auto now = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = now - start;
        if (elapsed.count() >= 5.0) {
            double fps = frame_count / elapsed.count();
            std::cout << ": Average FPS: " << fps << std::endl;
            start = now;
            frame_count = 0;
        }
    }
    return true;
}



int main()
{

    const char* url1 = "rtsp://admin:admin@192.168.1.103:554";
    const char* url2 = "rtsp://admin:admin@192.168.1.107:554";
    Init_uri( 0, url1);
    //Init_uri( 1,url2);

    std::thread thread1(test_cam_frame, 0);
    //std::thread thread2(test_cam_frame, 1);

    // 等待这两个线程完成
    thread1.join();
    //thread2.join();


}



