package org.mahoyo.port;
/** Steam's authentication interval is in seconds; coroutine delays are milliseconds. */
final class AuthTiming {
 static long pollMillis(float seconds){return seconds>0&&Float.isFinite(seconds)?Math.max(1000L,(long)Math.ceil(Math.min(seconds,300f)*1000.0)):5000L;}
 private AuthTiming(){}
}
