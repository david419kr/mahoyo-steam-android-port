#include <jni.h>
#include "asset_codecs.h"
#include <vector>

namespace {
void fail(JNIEnv* env,const char* message){if(!env->ExceptionCheck())env->ThrowNew(env->FindClass("java/io/IOException"),message);}
std::vector<uint8_t> bytes(JNIEnv* env,jbyteArray input){
    if(!input)throw mahoyo::assets::Error("Missing asset bytes");
    const auto size=env->GetArrayLength(input);if(size<0||size>256*1024*1024)throw mahoyo::assets::Error("Encoded asset budget");
    std::vector<uint8_t> result(size);env->GetByteArrayRegion(input,0,size,reinterpret_cast<jbyte*>(result.data()));return result;
}
mahoyo::assets::Bytes view(const std::vector<uint8_t>& value){return {value.data(),value.size()};}
}
extern "C" JNIEXPORT jintArray JNICALL Java_org_mahoyo_port_NativeAssetCodec_bounds(JNIEnv* env,jclass,jbyteArray input){
    try{auto data=bytes(env,input);if(env->ExceptionCheck())return nullptr;auto info=mahoyo::assets::imageInfo(view(data));jint values[]={jint(info.width),jint(info.height)};auto result=env->NewIntArray(2);if(result)env->SetIntArrayRegion(result,0,2,values);return result;}catch(const std::exception& error){fail(env,error.what());return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL Java_org_mahoyo_port_NativeAssetCodec_lenzu(JNIEnv* env,jclass,jbyteArray input){
    try{auto data=bytes(env,input);if(env->ExceptionCheck())return nullptr;auto raw=mahoyo::assets::decodeLenzu(view(data));auto result=env->NewByteArray(jsize(raw.size()));if(result)env->SetByteArrayRegion(result,0,jsize(raw.size()),reinterpret_cast<const jbyte*>(raw.data()));return result;}catch(const std::exception& error){fail(env,error.what());return nullptr;}
}
extern "C" JNIEXPORT jobject JNICALL Java_org_mahoyo_port_NativeAssetCodec_decode(JNIEnv* env,jclass,jbyteArray primary,jbyteArray patch,jint sample,jint left,jint top,jint right,jint bottom,jboolean premultiply){
    try{
        if(sample<1||left<0||top<0||right<=left||bottom<=top)throw mahoyo::assets::Error("Invalid native image crop");
        auto data=bytes(env,primary);if(env->ExceptionCheck())return nullptr;std::vector<uint8_t> overlay;if(patch)overlay=bytes(env,patch);if(env->ExceptionCheck())return nullptr;
        auto image=mahoyo::assets::decodeComposed(view(data),view(overlay),{uint32_t(left),uint32_t(top),uint32_t(right),uint32_t(bottom)},uint32_t(sample));
        std::vector<jint> argb(size_t(image.width)*image.height);for(size_t i=0;i<argb.size();++i){const auto* p=image.rgba.data()+i*4;argb[i]=jint(uint32_t(p[3])<<24|uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2]);}
        auto pixels=env->NewIntArray(jsize(argb.size()));if(!pixels)return nullptr;env->SetIntArrayRegion(pixels,0,jsize(argb.size()),argb.data());
        auto cls=env->FindClass("android/graphics/Bitmap"),config=env->FindClass("android/graphics/Bitmap$Config");
        auto format=env->GetStaticObjectField(config,env->GetStaticFieldID(config,"ARGB_8888","Landroid/graphics/Bitmap$Config;"));
        auto create=env->GetStaticMethodID(cls,"createBitmap","([IIILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
        auto result=env->CallStaticObjectMethod(cls,create,pixels,jint(image.width),jint(image.height),format);env->DeleteLocalRef(pixels);
        if(result&&!env->ExceptionCheck())env->CallVoidMethod(result,env->GetMethodID(cls,"setPremultiplied","(Z)V"),premultiply);
        return result;
    }catch(const std::exception& error){fail(env,error.what());return nullptr;}
}
