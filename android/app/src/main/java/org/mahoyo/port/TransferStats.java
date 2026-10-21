package org.mahoyo.port;

/** Session byte counters from the downloader can arrive out of order. */
final class TransferStats {
    long received,unpacked;
    private long sampleTime,sampleBytes,lastTraffic;
    private double rate;
    void reset(long now){received=unpacked=sampleBytes=0;rate=0;sampleTime=lastTraffic=now;}
    void update(long now,long network,long expanded){
        long previous=received;received=Math.max(received,network);unpacked=Math.max(unpacked,expanded);
        if(received>previous)lastTraffic=now;
        long elapsed=now-sampleTime;
        if(elapsed>=500){rate=(received-sampleBytes)*1000.0/elapsed;sampleTime=now;sampleBytes=received;}
    }
    double bytesPerSecond(long now){return now-lastTraffic>3000?0:rate;}
    static float overall(float sdk,long received,long unpacked,long expected){if(!Float.isFinite(sdk)||expected<=0)return Float.NaN;return (float)Math.max(0,Math.min(1,(sdk*expected+(double)unpacked-received)/expected));}
}
