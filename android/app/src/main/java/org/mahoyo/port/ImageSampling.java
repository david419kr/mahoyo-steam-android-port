package org.mahoyo.port;

/** Keep source detail when it fits both the GPU and the bounded decode budget. */
final class ImageSampling {
    static int sampleSize(int width,int height,int textureLimit,long maximumBytes){
        if(width<=0||height<=0||textureLimit<=0||maximumBytes<4)throw new IllegalArgumentException("Invalid image decode bounds");
        int sample=1;
        for(;;){
            long w=((long)width+sample-1)/sample,h=((long)height+sample-1)/sample;
            if(w<=textureLimit&&h<=textureLimit&&w*h<=maximumBytes/4)return sample;
            if(sample>=1<<30)throw new IllegalArgumentException("Image exceeds bounded sampling range");
            sample*=2;
        }
    }
}
