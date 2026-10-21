package org.mahoyo.port;
import org.junit.Test;import static org.junit.Assert.*;
public class AuthQrRetryTest {
 @Test public void aFailedInitialQrConnectionCanTryAnotherServer(){AuthQrRetry policy=new AuthQrRetry();policy.start(100);assertTrue(policy.retry(1000,"connecting",AuthQrRetry.DISCONNECTED));policy.clear();assertFalse(policy.retry(1001,"connecting",AuthQrRetry.DISCONNECTED));}
 @Test public void pendingQrReconnectsWithoutAnEndlessLoop(){AuthQrRetry policy=new AuthQrRetry();policy.start(100);for(int i=0;i<3;i++)assertTrue(policy.retry(1000,"qr",AuthQrRetry.DISCONNECTED));assertFalse(policy.retry(2000,"qr",AuthQrRetry.DISCONNECTED));}
 @Test public void userTimeoutAndCancelStopRenewal(){AuthQrRetry policy=new AuthQrRetry();policy.start(100);assertFalse(policy.retry(300100,"qr",AuthQrRetry.INTERRUPTED));policy.start(400000);assertTrue(policy.retry(400001,"qr",AuthQrRetry.INTERRUPTED));policy.clear();assertFalse(policy.retry(400002,"qr",AuthQrRetry.DISCONNECTED));}
 @Test public void approvedOrOtherOperationsNeverStartAnotherQr(){AuthQrRetry policy=new AuthQrRetry();policy.start(0);for(String stage:new String[]{"authenticating","ownership","choose","downloading"})assertFalse(policy.retry(1000,stage,AuthQrRetry.DISCONNECTED));assertFalse(policy.retry(1000,"qr","Account does not own game"));}
}
