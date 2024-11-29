#include <jni.h>
#include <string>
#include <sstream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/video.hpp>
#include <opencv2/videoio.hpp>   // Para VideoCapture
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "android/bitmap.h"

using namespace std;


cv::Mat generateBackground(int width, int height) {
    cv::Mat background(height, width, CV_8UC3);

    // Rellenar la imagen de fondo con un degradado en el espectro visible
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float normX = static_cast<float>(x) / width;

            float angle = normX * 2 * M_PI;
            uchar red = static_cast<uchar>(127.5 * (1 + sin(angle)));
            uchar green = static_cast<uchar>(127.5 * (1 + sin(angle + 2 * M_PI / 3)));
            uchar blue = static_cast<uchar>(127.5 * (1 + sin(angle + 4 * M_PI / 3)));

            background.at<cv::Vec3b>(y, x)[0] = blue;
            background.at<cv::Vec3b>(y, x)[1] = green;
            background.at<cv::Vec3b>(y, x)[2] = red;
        }
    }
    return background;
}

void bitmapToMat(JNIEnv * env, jobject bitmap, cv::Mat &dst, jboolean needUnPremultiplyAlpha){
    AndroidBitmapInfo info;
    void* pixels = 0;
    try {
        CV_Assert( AndroidBitmap_getInfo(env, bitmap, &info) >= 0 );
        CV_Assert( info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 ||
                   info.format == ANDROID_BITMAP_FORMAT_RGB_565 );
        CV_Assert( AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0 );
        CV_Assert( pixels );
        dst.create(info.height, info.width, CV_8UC4);
        if( info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 )
        {
            cv::Mat tmp(info.height, info.width, CV_8UC4, pixels);
            if(needUnPremultiplyAlpha) cvtColor(tmp, dst, cv::COLOR_mRGBA2RGBA);
            else tmp.copyTo(dst);
        } else {
// info.format == ANDROID_BITMAP_FORMAT_RGB_565
            cv::Mat tmp(info.height, info.width, CV_8UC2, pixels);
            cvtColor(tmp, dst, cv::COLOR_BGR5652RGBA);
        }
        AndroidBitmap_unlockPixels(env, bitmap);
        return;
    } catch(const cv::Exception& e) {
        AndroidBitmap_unlockPixels(env, bitmap);
//jclass je = env->FindClass("org/opencv/core/CvException");
        jclass je = env->FindClass("java/lang/Exception");
//if(!je) je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
        return;
    } catch (...) {
        AndroidBitmap_unlockPixels(env, bitmap);
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code {nBitmapToMat}");
        return;
    }
}
void matToBitmap(JNIEnv * env, cv::Mat src, jobject bitmap, jboolean needPremultiplyAlpha) {
    AndroidBitmapInfo info;
    void* pixels = 0;
    try {
        CV_Assert( AndroidBitmap_getInfo(env, bitmap, &info) >= 0 );
        CV_Assert( info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 ||
                   info.format == ANDROID_BITMAP_FORMAT_RGB_565 );
        CV_Assert( src.dims == 2 && info.height == (uint32_t)src.rows && info.width ==
                                                                         (uint32_t)src.cols );
        CV_Assert( src.type() == CV_8UC1 || src.type() == CV_8UC3 || src.type() == CV_8UC4 );
        CV_Assert( AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0 );
        CV_Assert( pixels );
        if( info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 )
        {
            cv::Mat tmp(info.height, info.width, CV_8UC4, pixels);
            if(src.type() == CV_8UC1)
            {
                cvtColor(src, tmp, cv::COLOR_GRAY2RGBA);
            } else if(src.type() == CV_8UC3){
                cvtColor(src, tmp, cv::COLOR_RGB2RGBA);
            } else if(src.type() == CV_8UC4){
                if(needPremultiplyAlpha) cvtColor(src, tmp, cv::COLOR_RGBA2mRGBA);
                else src.copyTo(tmp);
            }
        } else {
// info.format == ANDROID_BITMAP_FORMAT_RGB_565
            cv::Mat tmp(info.height, info.width, CV_8UC2, pixels);
            if(src.type() == CV_8UC1)
            {
                cvtColor(src, tmp, cv::COLOR_GRAY2BGR565);
            } else if(src.type() == CV_8UC3){
                cvtColor(src, tmp, cv::COLOR_RGB2BGR565);
            } else if(src.type() == CV_8UC4){
                cvtColor(src, tmp, cv::COLOR_RGBA2BGR565);
            }
        }
        AndroidBitmap_unlockPixels(env, bitmap);
        return;
    } catch(const cv::Exception& e) {
        AndroidBitmap_unlockPixels(env, bitmap);
//jclass je = env->FindClass("org/opencv/core/CvException");
        jclass je = env->FindClass("java/lang/Exception");
//if(!je) je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
        return;
    } catch (...) {
        AndroidBitmap_unlockPixels(env, bitmap);
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code {nMatToBitmap}");
        return;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_apppro_MainActivity_detectorBordes(JNIEnv* env, jobject /*this*/, jobject bitmapIn, jobject bitmapOut){
    AndroidBitmapInfo info;
    void *pixelsIn;

    AndroidBitmap_getInfo(env, bitmapIn, &info);
    AndroidBitmap_lockPixels(env, bitmapIn, &pixelsIn);

    cv::Mat frame(info.height, info.width, CV_8UC4, pixelsIn);
    bitmapToMat(env, bitmapIn, frame, false);
    cv::flip(frame, frame, 1);

    // Manejo de la imagen en OpenCV
    cv::Mat GRIS;
    cv::Mat background;
    cv::Mat adaptive_thresh;
    cv::Mat denoisedImage;
    cv::Mat framefinal;

    cv::Mat background2 = generateBackground(frame.cols, frame.rows);

    // Calcular FPS
    static double lastTime = 0.0;
    double currentTime = cv::getTickCount();
    double fps = cv::getTickFrequency() / (currentTime - lastTime);
    lastTime = currentTime;

    // Cambio a Gris
    cv::cvtColor(frame, GRIS, cv::COLOR_BGR2GRAY);

    // Umbral adaptativo
    cv::adaptiveThreshold(GRIS, adaptive_thresh, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 9, 2);

    // Desenfoque con medianBlur
    cv::medianBlur(adaptive_thresh, denoisedImage, 3);

    // Aplicar el fondo a los bordes
    framefinal = cv::Mat::zeros(background.size(), background.type());

    // Copiar usando la máscara
    background2.copyTo(framefinal, denoisedImage);

    // Dibujar el FPS en la esquina superior izquierda
    string fpsText = "FPS: " + to_string((int)fps);
    cv::putText(framefinal, fpsText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);

    // Convierte la imagen final a bitmap
    matToBitmap(env, framefinal, bitmapOut, false);

    // Liberar los recursos
    AndroidBitmap_unlockPixels(env, bitmapIn);
}
