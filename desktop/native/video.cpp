#include <jni.h>
#include <memory>
#include <stdexcept>
#include <string>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
namespace {
void check(int result,const char* operation){if(result<0){char message[AV_ERROR_MAX_STRING_SIZE]{};av_strerror(result,message,sizeof(message));throw std::runtime_error(std::string(operation)+": "+message);}}
void fail(JNIEnv* env,const char* message){if(!env->ExceptionCheck())env->ThrowNew(env->FindClass("java/io/IOException"),message);}
struct Decoder {
    AVFormatContext* format=nullptr;AVCodecContext* codec=nullptr;AVPacket* packet=nullptr;AVFrame* frame=nullptr;SwsContext* conversion=nullptr;
    int stream=-1,width=0,height=0;bool draining=false;int64_t duration=0,minimum=0,last=-1;AVRational rate{30,1};
    ~Decoder(){sws_freeContext(conversion);av_frame_free(&frame);av_packet_free(&packet);avcodec_free_context(&codec);avformat_close_input(&format);}
    void open(const char* url,int64_t seek){
        check(avformat_open_input(&format,url,nullptr,nullptr),"Open video");check(avformat_find_stream_info(format,nullptr),"Read video metadata");
        stream=av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);check(stream,"Find video stream");AVStream* video=format->streams[stream];
        const AVCodec* decoder=avcodec_find_decoder(video->codecpar->codec_id);if(!decoder)throw std::runtime_error("Video codec unavailable");
        codec=avcodec_alloc_context3(decoder);if(!codec)throw std::bad_alloc();check(avcodec_parameters_to_context(codec,video->codecpar),"Video parameters");codec->thread_count=2;check(avcodec_open2(codec,decoder,nullptr),"Initialize video codec");
        width=codec->width;height=codec->height;if(width<1||height<1||int64_t(width)*height>3840LL*2160)throw std::runtime_error("Unsupported video dimensions");
        rate=av_guess_frame_rate(format,video,nullptr);if(rate.num<=0||rate.den<=0)rate={30,1};
        duration=format->duration>0?format->duration/1000:av_rescale_q(video->duration,video->time_base,{1,1000});
        packet=av_packet_alloc();frame=av_frame_alloc();if(!packet||!frame)throw std::bad_alloc();minimum=seek;
        if(seek>0){check(av_seek_frame(format,stream,av_rescale_q(seek,{1,1000},video->time_base),AVSEEK_FLAG_BACKWARD),"Seek video");avcodec_flush_buffers(codec);}
    }
    int64_t next(uint8_t* output){
        for(;;){int result=avcodec_receive_frame(codec,frame);
            if(result==AVERROR_EOF)return -1;
            if(result>=0){
                int64_t pts=frame->best_effort_timestamp==AV_NOPTS_VALUE?(last<0?minimum:last+av_rescale_q(1,av_inv_q(rate),{1,1000})):av_rescale_q(frame->best_effort_timestamp,format->streams[stream]->time_base,{1,1000});
                last=pts;if(pts<minimum){av_frame_unref(frame);continue;}
                conversion=sws_getCachedContext(conversion,frame->width,frame->height,AVPixelFormat(frame->format),width,height,AV_PIX_FMT_RGBA,SWS_BILINEAR,nullptr,nullptr,nullptr);
                if(!conversion)throw std::runtime_error("Video color conversion unavailable");
                int space=frame->colorspace==AVCOL_SPC_BT709?SWS_CS_ITU709:SWS_CS_ITU601;const int* coefficients=sws_getCoefficients(space);
                check(sws_setColorspaceDetails(conversion,coefficients,frame->color_range==AVCOL_RANGE_JPEG,coefficients,1,0,1<<16,1<<16),"Video colorspace");
                uint8_t* planes[]{output,nullptr,nullptr,nullptr};int strides[]{width*4,0,0,0};int rows=sws_scale(conversion,frame->data,frame->linesize,0,frame->height,planes,strides);av_frame_unref(frame);
                if(rows!=height)throw std::runtime_error("Incomplete decoded video frame");return pts;
            }
            if(result!=AVERROR(EAGAIN))check(result,"Decode video frame");
            if(draining)return -1;
            do{result=av_read_frame(format,packet);if(result<0){check(avcodec_send_packet(codec,nullptr),"Drain video codec");draining=true;break;}if(packet->stream_index==stream){check(avcodec_send_packet(codec,packet),"Submit video packet");av_packet_unref(packet);break;}av_packet_unref(packet);}while(true);
        }
    }
};
}
extern "C" JNIEXPORT jlong JNICALL Java_org_mahoyo_port_NativeVideo_open(JNIEnv* env,jclass,jstring input,jlong seek){
    const char* url=env->GetStringUTFChars(input,nullptr);if(!url)return 0;
    try{auto decoder=std::make_unique<Decoder>();decoder->open(url,seek);env->ReleaseStringUTFChars(input,url);return jlong(reinterpret_cast<intptr_t>(decoder.release()));}catch(const std::exception& error){env->ReleaseStringUTFChars(input,url);fail(env,error.what());return 0;}
}
extern "C" JNIEXPORT jlongArray JNICALL Java_org_mahoyo_port_NativeVideo_info(JNIEnv* env,jclass,jlong handle){
    auto* d=reinterpret_cast<Decoder*>(intptr_t(handle));if(!d){fail(env,"Video decoder is closed");return nullptr;}
    jlong values[]{d->width,d->height,d->duration,d->rate.num,d->rate.den,av_find_best_stream(d->format,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0)>=0?1:0};auto result=env->NewLongArray(6);if(result)env->SetLongArrayRegion(result,0,6,values);return result;
}
extern "C" JNIEXPORT jlong JNICALL Java_org_mahoyo_port_NativeVideo_next(JNIEnv* env,jclass,jlong handle,jobject buffer){
    auto* d=reinterpret_cast<Decoder*>(intptr_t(handle));if(!d){fail(env,"Video decoder is closed");return -1;}
    auto* pixels=static_cast<uint8_t*>(env->GetDirectBufferAddress(buffer));if(!pixels||env->GetDirectBufferCapacity(buffer)<int64_t(d->width)*d->height*4){fail(env,"Invalid video output buffer");return -1;}
    try{return d->next(pixels);}catch(const std::exception& error){fail(env,error.what());return -1;}
}
extern "C" JNIEXPORT void JNICALL Java_org_mahoyo_port_NativeVideo_close(JNIEnv*,jclass,jlong handle){delete reinterpret_cast<Decoder*>(intptr_t(handle));}
