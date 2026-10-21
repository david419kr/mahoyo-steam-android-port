package org.mahoyo.port;
import org.junit.Test;import static org.junit.Assert.*;
public class TransferStatsTest {
 @Test public void rateUsesElapsedSecondsAndIdleRateDropsToZero(){TransferStats stats=new TransferStats();stats.reset(0);stats.update(1000,1000000,2000000);assertEquals(1000000,stats.bytesPerSecond(1000),.01);assertEquals(0,stats.bytesPerSecond(4001),.01);assertEquals(2000000,stats.unpacked);}
 @Test public void outOfOrderCallbacksAndNewTransferDoNotReuseOldProgress(){TransferStats stats=new TransferStats();stats.reset(0);stats.update(1000,1000,2000);stats.update(1100,900,1800);assertEquals(1000,stats.received);assertEquals(2000,stats.unpacked);stats.reset(2000);assertEquals(0,stats.received);assertEquals(0,stats.unpacked);assertEquals(0,stats.bytesPerSecond(2000),.01);}
}
