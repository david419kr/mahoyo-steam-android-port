package org.mahoyo.port;

/** Offline ownership and the online session required for downloading are separate. */
final class InstallRetry {
    static final int OPTIONS=0,QR=1,PASSWORD=2;
    static int action(int method,String stage,String error){
        if(method!=QR&&method!=PASSWORD||"PINNED_MANIFEST_UNAVAILABLE".equals(error))return OPTIONS;
        switch(stage){case "connecting":case "qr":case "authenticating":case "ownership":case "guard":case "mobile":case "downloading":return method;default:return OPTIONS;}
    }
}
