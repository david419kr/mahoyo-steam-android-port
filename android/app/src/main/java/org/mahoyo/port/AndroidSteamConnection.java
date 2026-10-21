package org.mahoyo.port;

import in.dragonbra.javasteam.networking.steam3.Connection;
import in.dragonbra.javasteam.networking.steam3.NetMsgEventArgs;
import in.dragonbra.javasteam.networking.steam3.ProtocolTypes;
import java.net.*;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import okhttp3.*;
import okio.ByteString;

/** Platform TLS supplies hostname-aware Android certificate validation. */
final class AndroidSteamConnection extends Connection {
    private final AtomicBoolean ended=new AtomicBoolean();
    private volatile WebSocket socket;
    private OkHttpClient client;
    private InetSocketAddress endpoint;
    @Override public void connect(InetSocketAddress address,int timeout){
        endpoint=address;
        client=new OkHttpClient.Builder().connectTimeout(timeout,TimeUnit.MILLISECONDS).pingInterval(20,TimeUnit.SECONDS).build();
        HttpUrl url=new HttpUrl.Builder().scheme("https").host(address.getHostString()).port(address.getPort()).addPathSegment("cmsocket").addPathSegment("").build();
        socket=client.newWebSocket(new Request.Builder().url(url).build(),new WebSocketListener(){
            @Override public void onOpen(WebSocket ws,Response response){socket=ws;if(ended.get()){ws.cancel();return;}onConnected();}
            @Override public void onMessage(WebSocket ws,ByteString bytes){if(!ended.get())onNetMsgReceived(new NetMsgEventArgs(bytes.toByteArray(),endpoint));}
            @Override public void onClosing(WebSocket ws,int code,String reason){disconnect(false);}
            @Override public void onClosed(WebSocket ws,int code,String reason){disconnect(false);}
            @Override public void onFailure(WebSocket ws,Throwable error,Response response){
                // Exception type only: account and protocol contents never enter logs.
                android.util.Log.w("MahoyoSteam","WebSocket: "+error.getClass().getSimpleName());disconnect(false);
            }
        });
        if(ended.get())socket.cancel();
    }
    @Override public void send(byte[] data){WebSocket ws=socket;if(!ended.get()&&(ws==null||!ws.send(ByteString.of(data))))disconnect(false);}
    @Override public void disconnect(boolean userInitiated){
        if(!ended.compareAndSet(false,true))return;
        WebSocket ws=socket;if(ws!=null)ws.cancel();
        if(client!=null){client.connectionPool().evictAll();client.dispatcher().executorService().shutdown();}
        onDisconnected(userInitiated);
    }
    @Override public InetAddress getLocalIP(){return InetAddress.getLoopbackAddress();}
    @Override public InetSocketAddress getCurrentEndPoint(){return endpoint;}
    @Override public ProtocolTypes getProtocolTypes(){return ProtocolTypes.WEB_SOCKET;}
}
