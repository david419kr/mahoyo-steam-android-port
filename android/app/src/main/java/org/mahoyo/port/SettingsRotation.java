package org.mahoyo.port;

/** Inherit the entry rotation; respond only to a later stable physical quarter-turn. */
final class SettingsRotation {
    private final boolean naturalLandscape;
    private int anchor=-1,candidate=-1;private long since;
    SettingsRotation(){this(false);}
    SettingsRotation(boolean naturalLandscape){this.naturalLandscape=naturalLandscape;}
    int sample(int degrees,long now){
        if(degrees<0||degrees>=360){candidate=-1;return -1;}
        int quadrant=((degrees+45)/90)%4,distance=Math.abs(degrees-quadrant*90);distance=Math.min(distance,360-distance);
        if(distance>20){candidate=-1;return -1;}
        if(anchor<0){anchor=quadrant;return -1;}
        if(quadrant==anchor){candidate=-1;return -1;}
        if(candidate!=quadrant){candidate=quadrant;since=now;return -1;}
        if(now-since<180)return -1;
        anchor=quadrant;candidate=-1;
        // Constrain the axis, leaving reverse rotation and hardware rotation maps to Android.
        return ((quadrant%2==0)!=naturalLandscape)?7:6;
    }
}
