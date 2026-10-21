package org.mahoyo.port;
/** Renew an unapproved QR connection without extending the user's five-minute login window. */
final class AuthQrRetry {
 static final String DISCONNECTED="STEAM_DISCONNECTED",INTERRUPTED="STEAM_REQUEST_INTERRUPTED";
 private long deadline;private int retries;
 void start(long now){deadline=now+300000;retries=0;}
 void clear(){deadline=0;retries=0;}
 boolean retry(long now,String stage,String message){if(!stage.equals("qr")||deadline<=now||retries>=3||!DISCONNECTED.equals(message)&&!INTERRUPTED.equals(message))return false;retries++;return true;}
}
