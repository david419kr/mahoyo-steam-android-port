package org.mahoyo.port;

import android.graphics.*;
import android.os.*;
import android.util.LruCache;
import android.widget.ImageView;
import java.io.File;
import java.lang.ref.WeakReference;
import java.util.concurrent.*;
import java.util.function.*;

/** One decoder, cancelled recycled cells, bounded sampled outputs and cache. */
final class FrontendImages {
    private static final class Token{volatile boolean cancelled;}
    private final Function<String,GameAsset> files;private final Consumer<String> errors;
    private final ThreadPoolExecutor decoder=new ThreadPoolExecutor(1,1,0,TimeUnit.MILLISECONDS,new LinkedBlockingQueue<>());
    private final Handler ui=new Handler(Looper.getMainLooper());private volatile boolean closed;private volatile long generation;
    private final LruCache<String,Bitmap> cache=new LruCache<String,Bitmap>(12*1024*1024){@Override protected int sizeOf(String key,Bitmap value){return value.getAllocationByteCount();}};
    FrontendImages(Function<String,GameAsset> files,Consumer<String> errors){this.files=files;this.errors=errors;}
    void bind(String name,int[] region,ImageView target,int width,int height,Runnable done){Object old=target.getTag();if(old instanceof Token)((Token)old).cancelled=true;Token token=new Token();target.setTag(token);target.setImageDrawable(null);long batch=generation;String key=name+":"+java.util.Arrays.toString(region)+":"+width+":"+height;Bitmap found=cache.get(key);if(found!=null){target.setImageBitmap(found);if(done!=null)done.run();return;}WeakReference<ImageView> view=new WeakReference<>(target);int[] crop=region==null?null:region.clone();
        decoder.execute(()->{if(closed||token.cancelled||batch!=generation)return;Bitmap bitmap=null;String failure=null;try{GameAsset file=files.apply(name);BitmapFactory.Options b=new BitmapFactory.Options();b.inJustDecodeBounds=true;file.bitmap(b);if(b.outWidth<=0||b.outHeight<=0||b.outWidth>32768||b.outHeight>32768)throw new IllegalArgumentException("Invalid image dimensions");Rect r=crop==null?new Rect(0,0,b.outWidth,b.outHeight):new Rect(crop[0],crop[1],crop[0]+crop[2],crop[1]+crop[3]);if(r.left<0||r.top<0||r.right>b.outWidth||r.bottom>b.outHeight||r.isEmpty())throw new IllegalArgumentException("Image crop outside source");BitmapFactory.Options o=new BitmapFactory.Options();o.inSampleSize=sample(r.width(),r.height(),width,height);o.inPreferredConfig=Bitmap.Config.ARGB_8888;
            if(crop==null)bitmap=file.bitmap(o);else bitmap=file.region(r,o);
            if(bitmap==null)throw new IllegalArgumentException("Image decoder returned no pixels");
        }catch(Exception e){failure="Extra image "+name+": "+e.getMessage();}if(closed||token.cancelled||batch!=generation)return;Bitmap result=bitmap;String error=failure;if(result!=null)cache.put(key,result);ui.post(()->{ImageView image=view.get();if(closed||token.cancelled||batch!=generation||image==null||image.getTag()!=token)return;if(result!=null)image.setImageBitmap(result);if(done!=null)done.run();if(error!=null)errors.accept(error);});});
    }
    static int sample(int w,int h,int targetW,int targetH){int sample=1;int tw=Math.max(1,Math.min(4096,targetW)),th=Math.max(1,Math.min(4096,targetH));while(w/sample>tw*2L||h/sample>th*2L||(long)(w/sample)*(h/sample)>8_000_000)sample*=2;return sample;}
    void invalidate(){generation++;decoder.getQueue().clear();cache.evictAll();}
    void close(){closed=true;invalidate();decoder.shutdownNow();ui.removeCallbacksAndMessages(null);}
}
