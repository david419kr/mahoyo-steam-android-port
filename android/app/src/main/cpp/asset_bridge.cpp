#include <jni.h>
#include <android/bitmap.h>
#include "asset_codecs.h"
#include <cstring>
#include <vector>
namespace {
void fail(JNIEnv* env,const char* text){if(!env->ExceptionCheck())env->ThrowNew(env->FindClass("java/io/IOException"),text);}
std::vector<uint8_t> bytes(JNIEnv* env,jbyteArray data){if(!data)throw mahoyo::assets::Error("Missing asset bytes");auto n=env->GetArrayLength(data);if(n<0||n>256*1024*1024)throw mahoyo::assets::Error("Encoded asset budget");std::vector<uint8_t> out(static_cast<size_t>(n));env->GetByteArrayRegion(data,0,n,reinterpret_cast<jbyte*>(out.data()));return out;}
mahoyo::assets::Bytes view(const std::vector<uint8_t>& b){return {b.data(),b.size()};}
struct Pixels {JNIEnv* env;jobject bitmap;void* data=nullptr;~Pixels(){if(data)AndroidBitmap_unlockPixels(env,bitmap);}};
}
extern "C" JNIEXPORT jintArray JNICALL Java_org_mahoyo_port_NativeAssetCodec_bounds(JNIEnv* env,jclass,jbyteArray data){try{auto b=bytes(env,data);if(env->ExceptionCheck())return nullptr;auto size=mahoyo::assets::imageInfo(view(b));jint dims[]{jint(size.width),jint(size.height)};auto out=env->NewIntArray(2);if(out)env->SetIntArrayRegion(out,0,2,dims);return out;}catch(const std::exception& e){fail(env,e.what());return nullptr;}}
extern "C" JNIEXPORT jbyteArray JNICALL Java_org_mahoyo_port_NativeAssetCodec_lenzu(JNIEnv* env,jclass,jbyteArray data){try{auto b=bytes(env,data);if(env->ExceptionCheck())return nullptr;auto raw=mahoyo::assets::decodeLenzu(view(b));auto out=env->NewByteArray(jsize(raw.size()));if(out)env->SetByteArrayRegion(out,0,jsize(raw.size()),reinterpret_cast<const jbyte*>(raw.data()));return out;}catch(const std::exception& e){fail(env,e.what());return nullptr;}}
extern "C" JNIEXPORT jobject JNICALL Java_org_mahoyo_port_NativeAssetCodec_decode(JNIEnv* env,jclass,jbyteArray primary,jbyteArray patch,jint sample,jint l,jint t,jint r,jint b,jboolean premultiply){try{
 if(sample<1||l<0||t<0||r<=l||b<=t)throw mahoyo::assets::Error("Invalid native image crop");auto encoded=bytes(env,primary);if(env->ExceptionCheck())return nullptr;
 auto image=mahoyo::assets::decodeImage(view(encoded),{uint32_t(l),uint32_t(t),uint32_t(r),uint32_t(b)});
 if(patch){auto p=bytes(env,patch);if(env->ExceptionCheck())return nullptr;auto info=mahoyo::assets::imageInfo(view(p));uint32_t right=std::min<uint32_t>(r,info.width),bottom=std::min<uint32_t>(b,info.height);if(uint32_t(l)<right&&uint32_t(t)<bottom){auto overlay=mahoyo::assets::decodeImage(view(p),{uint32_t(l),uint32_t(t),right,bottom});for(uint32_t y=0;y<overlay.height;++y)for(uint32_t x=0;x<overlay.width;++x){auto src=(size_t(y)*overlay.width+x)*4,dst=(size_t(y)*image.width+x)*4;if(overlay.rgba[src+3])std::memcpy(image.rgba.data()+dst,overlay.rgba.data()+src,4);}}}
 if(sample>1){uint32_t step=uint32_t(sample),w=uint32_t((uint64_t(image.width)+step-1)/step),h=uint32_t((uint64_t(image.height)+step-1)/step);std::vector<uint8_t> scaled(size_t(w)*h*4);
 for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){uint32_t left=x*step,top=y*step,right=uint32_t(std::min<uint64_t>(uint64_t(left)+step,image.width)),bottom=uint32_t(std::min<uint64_t>(uint64_t(top)+step,image.height));uint64_t sums[4]{},straight[3]{},count=uint64_t(right-left)*(bottom-top);
 for(uint32_t sy=top;sy<bottom;++sy)for(uint32_t sx=left;sx<right;++sx){auto* p=image.rgba.data()+(size_t(sy)*image.width+sx)*4;sums[3]+=p[3];for(int c=0;c<3;++c){sums[c]+=uint64_t(p[c])*p[3];straight[c]+=p[c];}}
 auto* p=scaled.data()+(size_t(y)*w+x)*4;for(int c=0;c<3;++c)p[c]=uint8_t(sums[3]?(sums[c]+sums[3]/2)/sums[3]:(straight[c]+count/2)/count);p[3]=uint8_t((sums[3]+count/2)/count);
 }image.width=w;image.height=h;image.rgba=std::move(scaled);}
 jclass cls=env->FindClass("android/graphics/Bitmap"),config=env->FindClass("android/graphics/Bitmap$Config");auto argb=env->GetStaticObjectField(config,env->GetStaticFieldID(config,"ARGB_8888","Landroid/graphics/Bitmap$Config;"));auto create=env->GetStaticMethodID(cls,"createBitmap","(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");auto bitmap=env->CallStaticObjectMethod(cls,create,jint(image.width),jint(image.height),argb);if(!bitmap||env->ExceptionCheck())return nullptr;
 env->CallVoidMethod(bitmap,env->GetMethodID(cls,"setPremultiplied","(Z)V"),premultiply);if(env->ExceptionCheck())return nullptr;
 AndroidBitmapInfo info{};if(AndroidBitmap_getInfo(env,bitmap,&info)!=ANDROID_BITMAP_RESULT_SUCCESS||info.format!=ANDROID_BITMAP_FORMAT_RGBA_8888)throw mahoyo::assets::Error("Invalid native bitmap output");Pixels locked{env,bitmap};if(AndroidBitmap_lockPixels(env,bitmap,&locked.data)!=ANDROID_BITMAP_RESULT_SUCCESS)throw mahoyo::assets::Error("Cannot lock native bitmap");
 for(uint32_t y=0;y<image.height;++y){auto* row=static_cast<uint8_t*>(locked.data)+size_t(y)*info.stride;auto* source=image.rgba.data()+size_t(y)*image.width*4;std::memcpy(row,source,size_t(image.width)*4);if(premultiply)for(uint32_t x=0;x<image.width;++x){auto* pixel=row+size_t(x)*4;for(int c=0;c<3;++c)pixel[c]=uint8_t((uint32_t(pixel[c])*pixel[3]+127)/255);}}
 return bitmap;
 }catch(const std::exception& e){fail(env,e.what());return nullptr;}}
