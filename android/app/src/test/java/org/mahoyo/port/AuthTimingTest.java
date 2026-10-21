package org.mahoyo.port;
import org.junit.Test;import static org.junit.Assert.*;
public class AuthTimingTest {
 @Test public void steamSecondsDoNotBecomeMillisecondPolling(){assertEquals(5000L,AuthTiming.pollMillis(5));assertEquals(2500L,AuthTiming.pollMillis(2.5f));}
 @Test public void invalidIntervalsCannotCauseATightLoop(){for(float value:new float[]{0,-1,Float.NaN,Float.POSITIVE_INFINITY})assertEquals(5000L,AuthTiming.pollMillis(value));assertEquals(1000L,AuthTiming.pollMillis(.01f));}
}
