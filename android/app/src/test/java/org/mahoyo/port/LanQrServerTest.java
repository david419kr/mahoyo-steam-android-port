package org.mahoyo.port;

import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

public final class LanQrServerTest {
    private static HttpURLConnection request(String base,String route)throws Exception{HttpURLConnection connection=(HttpURLConnection)new URL(base+route).openConnection(Proxy.NO_PROXY);connection.setConnectTimeout(2000);connection.setReadTimeout(3000);return connection;}
    @Test public void pageAndQrRefreshAreReadOnlyAndUncached()throws Exception{
        try(LanQrServer server=new LanQrServer(InetAddress.getByName("127.0.0.1"),"<Steam>","Scan with Steam","Ended")){
            HttpURLConnection page=request(server.url(),"");assertEquals(200,page.getResponseCode());assertEquals("no-store",page.getHeaderField("Cache-Control"));assertEquals("DENY",page.getHeaderField("X-Frame-Options"));String html=new String(page.getInputStream().readAllBytes(),StandardCharsets.UTF_8);assertTrue(html.contains("&lt;Steam&gt;"));assertTrue(html.contains("setTimeout(refresh,1000)"));page.disconnect();
            HttpURLConnection empty=request(server.url(),"qr.png");assertEquals(503,empty.getResponseCode());empty.disconnect();
            byte[] first={1,2,3};server.update(first);first[0]=9;
            HttpURLConnection qr=request(server.url(),"qr.png?r=1");assertEquals("image/png",qr.getContentType());assertArrayEquals(new byte[]{1,2,3},qr.getInputStream().readAllBytes());qr.disconnect();
            server.update(new byte[]{4,5});HttpURLConnection status=request(server.url(),"status");assertEquals("{\"revision\":2,\"ready\":true}",new String(status.getInputStream().readAllBytes(),StandardCharsets.UTF_8));status.disconnect();
            server.update(null);HttpURLConnection clearing=request(server.url(),"status");assertTrue(new String(clearing.getInputStream().readAllBytes(),StandardCharsets.UTF_8).contains("\"ready\":false"));clearing.disconnect();
            HttpURLConnection missing=request(server.url(),"installation.json");assertEquals(404,missing.getResponseCode());missing.disconnect();
            HttpURLConnection post=request(server.url(),"status");post.setRequestMethod("POST");assertEquals(405,post.getResponseCode());post.disconnect();
        }
    }
    @Test public void foreignHostsAndOriginsAreRejectedAndCloseStopsListening()throws Exception{
        LanQrServer server=new LanQrServer(InetAddress.getByName("127.0.0.1"),"Steam","Scan","Ended");URI uri=new URI(server.url());int port=uri.getPort();
        try{
            try(Socket socket=new Socket(uri.getHost(),port)){socket.setSoTimeout(3000);socket.getOutputStream().write("GET / HTTP/1.1\r\nHost: example.invalid\r\n\r\n".getBytes(StandardCharsets.US_ASCII));assertTrue(new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.US_ASCII)).readLine().contains("403"));}
            try(Socket socket=new Socket(uri.getHost(),port)){socket.setSoTimeout(3000);socket.getOutputStream().write(("GET /status HTTP/1.1\r\nHost: "+uri.getAuthority()+"\r\nOrigin: https://example.invalid\r\n\r\n").getBytes(StandardCharsets.US_ASCII));assertTrue(new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.US_ASCII)).readLine().contains("403"));}
            try{server.update(new byte[256*1024+1]);fail("oversized QR accepted");}catch(IllegalArgumentException expected){}
        }finally{server.close();server.close();}
        try(Socket ignored=new Socket(uri.getHost(),port)){fail("closed server accepted a connection");}catch(ConnectException expected){}
    }
}
