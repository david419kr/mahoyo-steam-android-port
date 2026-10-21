package org.mahoyo.port;

/** Last deliberate movement wins; small pointer jitter does not reverse intent. */
final class DragIntent {
    private float extreme,slop;private boolean forward=true;
    void start(float value,float density){extreme=value;slop=4*density;forward=true;}
    void move(float value){if(forward){if(value>extreme)extreme=value;else if(extreme-value>=slop){forward=false;extreme=value;}}else{if(value<extreme)extreme=value;else if(value-extreme>=slop){forward=true;extreme=value;}}}
    boolean forward(){return forward;}
}
