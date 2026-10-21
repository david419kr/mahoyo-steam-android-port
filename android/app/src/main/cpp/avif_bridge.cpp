#include <jni.h>
#include <android/bitmap.h>
#include <avif/avif.h>
#include "avif_decode_plan.h"
#include <memory>
#include <string>
#include <vector>

namespace {
using Decoder=std::unique_ptr<avifDecoder,decltype(&avifDecoderDestroy)>;
using Image=std::unique_ptr<avifImage,decltype(&avifImageDestroy)>;
void fail(JNIEnv* env,const char* text){if(!env->ExceptionCheck())env->ThrowNew(env->FindClass("java/io/IOException"),text);}
void require(avifResult result,const avifDecoder* decoder){if(result!=AVIF_RESULT_OK)throw std::runtime_error(std::string("AVIF ")+avifResultToString(result)+": "+decoder->diag.error);}
std::vector<uint8_t> bytes(JNIEnv* env,jbyteArray data){
    if(!data)throw std::runtime_error("Missing AVIF bytes");auto n=env->GetArrayLength(data);
    if(n<16||n>64*1024*1024)throw std::runtime_error("AVIF encoded image budget");
    std::vector<uint8_t> value(static_cast<size_t>(n));env->GetByteArrayRegion(data,0,n,reinterpret_cast<jbyte*>(value.data()));return value;
}
Decoder parse(const std::vector<uint8_t>& data){
    Decoder decoder(avifDecoderCreate(),avifDecoderDestroy);if(!decoder)throw std::runtime_error("Cannot create AVIF decoder");
    decoder->codecChoice=AVIF_CODEC_CHOICE_DAV1D;decoder->maxThreads=2;decoder->imageSizeLimit=mahoyo::avif_plan::sourcePixels;decoder->imageDimensionLimit=16384;decoder->imageCountLimit=1;
    decoder->ignoreExif=AVIF_TRUE;decoder->ignoreXMP=AVIF_TRUE;
    require(avifDecoderSetIOMemory(decoder.get(),data.data(),data.size()),decoder.get());require(avifDecoderParse(decoder.get()),decoder.get());
    mahoyo::avif_plan::bounds(decoder->image->width,decoder->image->height);
    if(decoder->imageCount!=1||decoder->image->depth!=8)throw std::runtime_error("Expected one 8-bit AVIF image");return decoder;
}
jobject bitmap(JNIEnv* env,uint32_t w,uint32_t h,bool premultiply){
    jclass config=env->FindClass("android/graphics/Bitmap$Config");jobject argb=env->GetStaticObjectField(config,env->GetStaticFieldID(config,"ARGB_8888","Landroid/graphics/Bitmap$Config;"));
    jclass cls=env->FindClass("android/graphics/Bitmap");auto create=env->GetStaticMethodID(cls,"createBitmap","(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject result=env->CallStaticObjectMethod(cls,create,jint(w),jint(h),argb);
    if(result&&!env->ExceptionCheck())env->CallVoidMethod(result,env->GetMethodID(cls,"setPremultiplied","(Z)V"),jboolean(premultiply));
    return result;
}
struct Pixels {JNIEnv* env;jobject bitmap;void* data=nullptr;~Pixels(){if(data)AndroidBitmap_unlockPixels(env,bitmap);}};
}
extern "C" JNIEXPORT jintArray JNICALL Java_org_mahoyo_port_AvifCodec_bounds(JNIEnv* env,jclass,jbyteArray encoded){
    try{auto input=bytes(env,encoded);if(env->ExceptionCheck())return nullptr;auto decoder=parse(input);jint dims[]{jint(decoder->image->width),jint(decoder->image->height)};auto result=env->NewIntArray(2);if(result)env->SetIntArrayRegion(result,0,2,dims);return result;}
    catch(const std::exception& e){fail(env,e.what());return nullptr;}
}
extern "C" JNIEXPORT jobject JNICALL Java_org_mahoyo_port_AvifCodec_decode(JNIEnv* env,jclass,jbyteArray encoded,jint sample,jboolean crop,jint l,jint t,jint r,jint b,jboolean premultiply){
    try{
        auto input=bytes(env,encoded);if(env->ExceptionCheck())return nullptr;auto decoder=parse(input);
        auto plan=mahoyo::avif_plan::make(decoder->image->width,decoder->image->height,sample,crop,l,t,r,b);
        require(avifDecoderNextImage(decoder.get()),decoder.get());
        if(decoder->image->width!=plan.width||decoder->image->height!=plan.height)require(avifImageScale(decoder->image,plan.width,plan.height,&decoder->diag),decoder.get());
        Image view(avifImageCreateEmpty(),avifImageDestroy);if(!view)throw std::runtime_error("Cannot create AVIF crop view");
        avifCropRect rect{plan.left,plan.top,plan.right-plan.left,plan.bottom-plan.top};require(avifImageSetViewRect(view.get(),decoder->image,&rect),decoder.get());
        jobject result=bitmap(env,rect.width,rect.height,premultiply);if(!result||env->ExceptionCheck())return nullptr;
        AndroidBitmapInfo info{};if(AndroidBitmap_getInfo(env,result,&info)!=ANDROID_BITMAP_RESULT_SUCCESS||info.format!=ANDROID_BITMAP_FORMAT_RGBA_8888)throw std::runtime_error("Invalid AVIF output bitmap");
        Pixels pixels{env,result};if(AndroidBitmap_lockPixels(env,result,&pixels.data)!=ANDROID_BITMAP_RESULT_SUCCESS)throw std::runtime_error("Cannot lock AVIF bitmap");
        avifRGBImage rgb;avifRGBImageSetDefaults(&rgb,view.get());rgb.depth=8;rgb.format=AVIF_RGB_FORMAT_RGBA;rgb.alphaPremultiplied=premultiply?AVIF_TRUE:AVIF_FALSE;rgb.rowBytes=info.stride;rgb.pixels=static_cast<uint8_t*>(pixels.data);
        require(avifImageYUVToRGB(view.get(),&rgb),decoder.get());return result;
    }catch(const std::exception& e){fail(env,e.what());return nullptr;}
}
