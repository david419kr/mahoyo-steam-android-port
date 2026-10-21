package org.mahoyo.port;

/** Restoring the opt-out affects the next launch, never interrupts the current session. */
final class ReaderHelpSession {
    private final boolean suppressedAtLaunch;private boolean shown;
    ReaderHelpSession(boolean suppressedAtLaunch){this.suppressedAtLaunch=suppressedAtLaunch;}
    boolean take(boolean suppressedNow){if(shown||suppressedAtLaunch||suppressedNow)return false;shown=true;return true;}
}
