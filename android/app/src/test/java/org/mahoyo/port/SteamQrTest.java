package org.mahoyo.port;
import org.junit.Test;import static org.junit.Assert.*;import com.google.zxing.*;import com.google.zxing.common.*;
public final class SteamQrTest {
 @Test public void challengeRoundTrip()throws Exception{String url="https://s.team/q/1/1234567890123456789";BitMatrix matrix=SteamQr.encode(url,320);int[] pixels=new int[320*320];for(int y=0;y<320;y++)for(int x=0;x<320;x++)pixels[y*320+x]=matrix.get(x,y)?0xff000000:0xffffffff;BinaryBitmap bitmap=new BinaryBitmap(new HybridBinarizer(new RGBLuminanceSource(320,320,pixels)));assertEquals(url,new MultiFormatReader().decode(bitmap).getText());}
 @Test public void unrelatedLoginSitesAreRejected()throws Exception{for(String url:new String[]{"http://s.team/q/1/1","https://example.com/q/1/1","https://s.team.example.com/q/1/1","https://account@s.team/q/1/1","javascript:login()"})try{SteamQr.encode(url,240);fail(url);}catch(IllegalArgumentException expected){}}
}
