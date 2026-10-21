#include <jni.h>
#include <cstring>
#include "runtime_api.h"

namespace {
struct Utf8 {
    JNIEnv* env; jstring source; const char* data;
    Utf8(JNIEnv* e,jstring s):env(e),source(s),data(s?e->GetStringUTFChars(s,nullptr):nullptr){}
    ~Utf8(){if(data)env->ReleaseStringUTFChars(source,data);}
    const char* get()const{return data?data:"";}
};
jstring result(JNIEnv* env,const char* value){
    if(!value)value="{}";
    const auto size=static_cast<jsize>(std::strlen(value));
    jbyteArray bytes=env->NewByteArray(size);
    env->SetByteArrayRegion(bytes,0,size,reinterpret_cast<const jbyte*>(value));
    jclass strings=env->FindClass("java/lang/String");
    jmethodID constructor=env->GetMethodID(strings,"<init>","([BLjava/lang/String;)V");
    jstring charset=env->NewStringUTF("UTF-8");
    auto copy=static_cast<jstring>(env->NewObject(strings,constructor,bytes,charset));
    env->DeleteLocalRef(bytes);env->DeleteLocalRef(strings);env->DeleteLocalRef(charset);
    return copy;
}
}
extern "C" JNIEXPORT jlong JNICALL Java_org_mahoyo_port_NativeBridge_create(JNIEnv* env,jclass,jstring root){Utf8 path(env,root);return static_cast<jlong>(woh_create(path.get()));}
extern "C" JNIEXPORT jlong JNICALL Java_org_mahoyo_port_NativeBridge_createConfigured(JNIEnv* env,jclass,jstring root,jstring edition,jstring language){Utf8 a(env,root),b(env,edition),c(env,language);return static_cast<jlong>(woh_create_configured(a.get(),b.get(),c.get()));}
extern "C" JNIEXPORT jstring JNICALL Java_org_mahoyo_port_NativeBridge_command(JNIEnv* env,jclass,jlong handle,jstring type,jstring payload){Utf8 a(env,type),b(env,payload);return result(env,woh_command(static_cast<uint64_t>(handle),a.get(),b.get()));}
extern "C" JNIEXPORT void JNICALL Java_org_mahoyo_port_NativeBridge_waveClock(JNIEnv*,jclass,jlong handle,jlong uptimeMs){if(uptimeMs>=0)woh_wave_clock(static_cast<uint64_t>(handle),static_cast<uint64_t>(uptimeMs));}
extern "C" JNIEXPORT jstring JNICALL Java_org_mahoyo_port_NativeBridge_snapshot(JNIEnv* env,jclass,jlong handle){return result(env,woh_snapshot(static_cast<uint64_t>(handle)));}
extern "C" JNIEXPORT void JNICALL Java_org_mahoyo_port_NativeBridge_destroy(JNIEnv*,jclass,jlong handle){woh_destroy(static_cast<uint64_t>(handle));}
