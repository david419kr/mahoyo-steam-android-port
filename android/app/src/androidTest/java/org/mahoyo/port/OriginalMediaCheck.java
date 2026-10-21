package org.mahoyo.port;

import android.content.Context;
import android.media.*;
import android.os.SystemClock;
import java.io.IOException;
import java.nio.ByteBuffer;
import org.json.JSONObject;

/** Decode actual archive ranges without extracting full media or producing audible output. */
final class OriginalMediaCheck {
    static String run(Context context)throws Exception{
        Installation install=new Installation(context);if(!install.ready()||!install.dataAvailable())throw new IOException("A real owned installation is required");
        JSONObject record=install.read();if(!record.getString("backend").equals("steam"))throw new IOException("Original Steam backend required");
        return run(new GameResources(context,install.dataRoot(record),install.archive(record),record.getString("edition")),record.getString("edition"));
    }
    static String run(GameResources resources,String edition)throws Exception{
        int tracks=0;
        for(String name:new String[]{"a40_4_1_0026.hw","ssedecided.hw","m02.hw"}){decode(resources.file("audio/"+name+".ogg"),"audio/",true);++tracks;}
        decode(resources.file("video/logo.mp4"),"video/",true);++tracks;
        for(String language:LanguagePolicy.choices(edition.equals("korean"))){
            if(language.equals("auto"))continue;resources.setLanguage(language);GameAsset movie=resources.file("video/opening_ja.mp4");decode(movie,"video/",true);++tracks;if(decode(movie,"audio/",false))++tracks;
        }
        return "Archive media: "+tracks+" audio/video tracks decoded through Android MediaCodec, including localized openings. No full media cache or audible playback was used.\n";
    }
    private static boolean decode(GameAsset asset,String prefix,boolean required)throws Exception{
        MediaExtractor extractor=new MediaExtractor();MediaCodec codec=null;boolean started=false;
        try{
            asset.source(extractor);MediaFormat format=null;
            for(int i=0;i<extractor.getTrackCount();++i){MediaFormat candidate=extractor.getTrackFormat(i);if(candidate.getString(MediaFormat.KEY_MIME).startsWith(prefix)){format=candidate;extractor.selectTrack(i);break;}}
            if(format==null){if(required)throw new IOException("Required media track missing: "+prefix);return false;}
            String decoder=new MediaCodecList(MediaCodecList.REGULAR_CODECS).findDecoderForFormat(format);if(decoder==null)throw new IOException("No decoder for "+format.getString(MediaFormat.KEY_MIME));
            codec=MediaCodec.createByCodecName(decoder);codec.configure(format,null,null,0);codec.start();started=true;
            MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();boolean eos=false;int frames=0;long deadline=SystemClock.elapsedRealtime()+10000;
            while(frames<2&&SystemClock.elapsedRealtime()<deadline){
                if(!eos){int slot=codec.dequeueInputBuffer(10000);if(slot>=0){ByteBuffer input=codec.getInputBuffer(slot);if(input==null)throw new IOException("No media input buffer");input.clear();int size=extractor.readSampleData(input,0);
                    if(size<0){codec.queueInputBuffer(slot,0,0,0,MediaCodec.BUFFER_FLAG_END_OF_STREAM);eos=true;}
                    else{codec.queueInputBuffer(slot,0,size,Math.max(0,extractor.getSampleTime()),0);extractor.advance();}
                }}
                int slot=codec.dequeueOutputBuffer(info,10000);if(slot>=0){if(info.size>0)++frames;codec.releaseOutputBuffer(slot,false);if((info.flags&MediaCodec.BUFFER_FLAG_END_OF_STREAM)!=0)break;}
            }
            if(frames<2)throw new IOException("Archive media decode did not produce two buffers: "+prefix);return true;
        }finally{if(codec!=null){try{if(started)codec.stop();}finally{codec.release();}}extractor.release();}
    }
}
