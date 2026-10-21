package org.mahoyo.port;
import org.junit.Test;import static org.junit.Assert.*;
public final class InstallSpaceTest {
    @Test public void importNeedsAnotherCopyAndReserve(){long size=5_000_000_000L,reserve=InstallSpace.OBB_RESERVE;assertFalse(InstallSpace.enough(size,size,0,reserve));assertFalse(InstallSpace.enough(size+reserve-1,size,0,reserve));assertTrue(InstallSpace.enough(size+reserve,size,0,reserve));}
    @Test public void resumeCreditsOnlyAllocatedBytes(){long total=17_702_837_564L,allocated=1_000_000_000L,reserve=InstallSpace.STEAM_RESERVE;assertTrue(InstallSpace.enough(total-allocated+reserve,total,allocated,reserve));assertFalse(InstallSpace.enough(total-allocated+reserve,total,0,reserve));assertFalse(InstallSpace.enough(reserve-1,total,total,reserve));}
    @Test public void extremeAndUnknownValuesCannotBypassReserve(){assertFalse(InstallSpace.enough(Long.MAX_VALUE,Long.MAX_VALUE,0,1));assertFalse(InstallSpace.enough(-1,0,0,1));assertTrue(InstallSpace.enough(1,100,200,1));assertFalse(InstallSpace.enough(100,100,-100,1));}
}
