package org.mahoyo.port;
import com.google.zxing.*;import com.google.zxing.common.BitMatrix;import com.google.zxing.qrcode.QRCodeWriter;import com.google.zxing.qrcode.decoder.ErrorCorrectionLevel;import java.net.URI;import java.util.EnumMap;
/** QR payloads come from Steam's authenticated session, never a third-party login page. */
final class SteamQr {
 static BitMatrix encode(String challenge,int size)throws Exception{
  URI uri=new URI(challenge);if(challenge.length()>2048||!"https".equalsIgnoreCase(uri.getScheme())||!"s.team".equalsIgnoreCase(uri.getHost())||uri.getRawUserInfo()!=null||!uri.getPath().startsWith("/q/"))throw new IllegalArgumentException("Unexpected Steam QR challenge");
  EnumMap<EncodeHintType,Object> hints=new EnumMap<>(EncodeHintType.class);hints.put(EncodeHintType.MARGIN,4);hints.put(EncodeHintType.ERROR_CORRECTION,ErrorCorrectionLevel.M);return new QRCodeWriter().encode(challenge,BarcodeFormat.QR_CODE,size,size,hints);
 }
 private SteamQr(){}
}
