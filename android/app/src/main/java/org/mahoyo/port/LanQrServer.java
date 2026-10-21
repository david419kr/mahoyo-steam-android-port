package org.mahoyo.port;

import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

/** Temporary read-only QR display, bound to one explicit LAN address. No account input or files. */
final class LanQrServer implements Closeable {
    private final ServerSocket server=new ServerSocket();
    private final Set<Socket> clients=ConcurrentHashMap.newKeySet();
    private final ThreadPoolExecutor workers=new ThreadPoolExecutor(2,2,0,TimeUnit.MILLISECONDS,new ArrayBlockingQueue<>(4),r->{Thread t=new Thread(r,"steam-qr-http");t.setDaemon(true);return t;});
    private final String authority;private final byte[] page;private volatile boolean closed;
    private static final class Frame {final long revision;final byte[] png;Frame(long revision,byte[] png){this.revision=revision;this.png=png;}}
    private volatile Frame frame=new Frame(0,null);
    LanQrServer(InetAddress address,String title,String guide,String ended)throws IOException{
        try{server.bind(new InetSocketAddress(address,0),4);}catch(IOException failure){workers.shutdownNow();server.close();throw failure;}
        authority=address.getHostAddress()+":"+server.getLocalPort();
        page=html(title,guide,ended).getBytes(StandardCharsets.UTF_8);
        Thread acceptor=new Thread(()->{
            while(!closed)try{Socket client=server.accept();clients.add(client);try{workers.execute(()->serve(client));}catch(RejectedExecutionException busy){clients.remove(client);client.close();}}
            catch(IOException failure){if(!closed)close();}
        },"steam-qr-listen");acceptor.setDaemon(true);acceptor.start();
    }
    String url(){return "http://"+authority+"/";}
    synchronized void update(byte[] png){if(closed)return;if(png!=null&&png.length>256*1024)throw new IllegalArgumentException("QR image budget");frame=new Frame(frame.revision+1,png==null?null:png.clone());}
    private void serve(Socket client){
        try(Socket socket=client){
            socket.setSoTimeout(2000);InputStream input=socket.getInputStream();ByteArrayOutputStream header=new ByteArrayOutputStream();int tail=0;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(2);
            while(header.size()<8192&&System.nanoTime()<deadline){int b=input.read();if(b<0)return;header.write(b);tail=(tail<<8)|b;if(tail==0x0d0a0d0a)break;}
            if(tail!=0x0d0a0d0a){respond(socket,431,"text/plain","Header too large".getBytes(StandardCharsets.UTF_8));return;}
            String[] lines=header.toString(StandardCharsets.US_ASCII.name()).split("\r\n");String[] request=lines[0].split(" ",3);
            if(request.length!=3){respond(socket,400,"text/plain",new byte[0]);return;}
            String host="",origin="";for(int i=1;i<lines.length;i++){int at=lines[i].indexOf(':');if(at<0)continue;String name=lines[i].substring(0,at).trim(),value=lines[i].substring(at+1).trim();if(name.equalsIgnoreCase("Host"))host=value;if(name.equalsIgnoreCase("Origin"))origin=value;}
            if(!authority.equalsIgnoreCase(host)||!origin.isEmpty()&&!origin.equalsIgnoreCase("http://"+authority)){respond(socket,403,"text/plain",new byte[0]);return;}
            if(!request[0].equals("GET")){respond(socket,405,"text/plain",new byte[0]);return;}
            String path=request[1].split("\\?",2)[0];Frame current=frame;
            if(closed)return;
            if(path.equals("/"))respond(socket,200,"text/html; charset=utf-8",page);
            else if(path.equals("/status"))respond(socket,200,"application/json; charset=utf-8",("{\"revision\":"+current.revision+",\"ready\":"+(current.png!=null)+"}").getBytes(StandardCharsets.UTF_8));
            else if(path.equals("/qr.png")&&current.png!=null)respond(socket,200,"image/png",current.png);
            else respond(socket,path.equals("/qr.png")?503:404,"text/plain",new byte[0]);
        }catch(IOException ignored){}finally{clients.remove(client);}
    }
    private static void respond(Socket socket,int code,String type,byte[] body)throws IOException{
        String reason=code==200?"OK":code==400?"Bad Request":code==403?"Forbidden":code==404?"Not Found":code==405?"Method Not Allowed":code==431?"Request Header Fields Too Large":"Service Unavailable";
        OutputStream output=socket.getOutputStream();output.write(("HTTP/1.1 "+code+" "+reason+"\r\nContent-Type: "+type+"\r\nContent-Length: "+body.length+"\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\n\r\n").getBytes(StandardCharsets.US_ASCII));output.write(body);output.flush();
    }
    private static String escape(String text){return text.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;").replace("\"","&quot;");}
    private static String html(String title,String guide,String ended){return "<!doctype html><html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>"+escape(title)+"</title><style>body{margin:0;min-height:100vh;display:grid;place-items:center;background:#0b1423;color:#edf3fa;font:16px system-ui}main{max-width:560px;padding:24px;text-align:center}h1{font-size:22px}img{width:min(78vw,440px);height:auto;background:white;padding:16px;border-radius:12px;image-rendering:pixelated}p{line-height:1.6;color:#b7cce1}</style><main><h1>"+escape(title)+"</h1><img id='qr' hidden alt='Steam QR'><p id='status'>"+escape(guide)+"</p><p id='ended' hidden>"+escape(ended)+"</p></main><script>let revision=-1;const qr=document.getElementById('qr'),status=document.getElementById('status'),guide=status.textContent;async function refresh(){try{const response=await fetch('/status',{cache:'no-store'});if(!response.ok)throw Error();const s=await response.json();qr.hidden=!s.ready;if(s.ready&&s.revision!==revision){revision=s.revision;qr.src='/qr.png?r='+revision;}status.textContent=guide;}catch(e){qr.hidden=true;status.textContent=document.getElementById('ended').textContent;}setTimeout(refresh,1000);}refresh();</script></html>";}
    @Override public synchronized void close(){closed=true;frame=new Frame(frame.revision+1,null);try{server.close();}catch(IOException ignored){}for(Socket client:clients)try{client.close();}catch(IOException ignored){}clients.clear();workers.shutdownNow();}
}
