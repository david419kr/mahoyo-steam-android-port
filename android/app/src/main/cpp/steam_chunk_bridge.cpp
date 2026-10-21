#include <jni.h>
#include <cstdint>
#include <cstdlib>
#include "LzmaDec.h"

namespace {
void* allocate(ISzAllocPtr, size_t size) { return std::malloc(size); }
void release(ISzAllocPtr, void* memory) { std::free(memory); }
const ISzAlloc allocator{allocate, release};
uint32_t little(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
struct Bytes {
    JNIEnv* env; jbyteArray array; jbyte* data; jint mode;
    Bytes(JNIEnv* e, jbyteArray a, jint m):env(e),array(a),data(e->GetByteArrayElements(a,nullptr)),mode(m){}
    ~Bytes(){if(data)env->ReleaseByteArrayElements(array,data,mode);}
};
int invalid(JNIEnv* env) {
    if(!env->ExceptionCheck())env->ThrowNew(env->FindClass("java/io/IOException"),"Invalid Steam VZip/LZMA chunk");
    return 0;
}
}

extern "C" JNIEXPORT jint JNICALL Java_org_mahoyo_port_SteamChunkCodec_decodeVzip(JNIEnv* env,jclass,jbyteArray input,jint length,jbyteArray output) {
    // Reject unreasonable sizes before allocating or pinning Java arrays.
    if(!input || !output || env->IsSameObject(input,output) || length<22 || length>16*1024*1024 || length>env->GetArrayLength(input))return invalid(env);
    const jint capacity=env->GetArrayLength(output);
    if(capacity<0 || capacity>16*1024*1024)return invalid(env);
    Bytes encoded(env,input,JNI_ABORT);if(!encoded.data)return 0;
    auto* data=reinterpret_cast<const uint8_t*>(encoded.data);
    uint32_t expected=little(data+length-6),dictionary=little(data+8);
    if(data[0]!='V'||data[1]!='Z'||data[2]!='a'||data[length-2]!='z'||data[length-1]!='v'||expected>uint32_t(capacity)||dictionary>64*1024*1024)return invalid(env);
    Bytes decoded(env,output,0);if(!decoded.data)return 0;
    SizeT written=expected, consumed=SizeT(length-22);ELzmaStatus status=LZMA_STATUS_NOT_SPECIFIED;
    SRes result=LzmaDecode(reinterpret_cast<Byte*>(decoded.data),&written,data+12,&consumed,data+7,5,LZMA_FINISH_END,&status,&allocator);
    if(result!=SZ_OK || written!=expected || consumed!=SizeT(length-22) || (status!=LZMA_STATUS_FINISHED_WITH_MARK && status!=LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK))return invalid(env);
    return jint(written);
}
