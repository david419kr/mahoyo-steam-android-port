package org.mahoyo.port;

/** Recover the ornament's light contribution without its original black backdrop. */
final class DialoguePixels {
    static void removeBackdrop(int[] pixels){
        for(int i=0;i<pixels.length;i++){
            int p=pixels[i],r=(p>>>16)&255,g=(p>>>8)&255,b=p&255,peak=Math.max(r,Math.max(g,b)),denominator=Math.max(1,peak);
            int a=(int)Math.rint((p>>>24)*peak/255.0);
            pixels[i]=(a<<24)|((int)Math.rint(r*255.0/denominator)<<16)|((int)Math.rint(g*255.0/denominator)<<8)|(int)Math.rint(b*255.0/denominator);
        }
    }
    private DialoguePixels(){}
}
