package org.mahoyo.port;
import org.junit.Test;
import static org.junit.Assert.*;
public final class DialoguePixelsTest {
    @Test public void preservesLightContributionAndAlpha(){int[] pixels={0xff204080,0xff000000,0x80808080};DialoguePixels.removeBackdrop(pixels);assertArrayEquals(new int[]{0x804080ff,0x00000000,0x40ffffff},pixels);}
}
