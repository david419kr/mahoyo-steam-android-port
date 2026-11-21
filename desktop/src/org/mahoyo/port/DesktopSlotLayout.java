package org.mahoyo.port;

import java.util.List;

/** Bounded two-line cards; twelve slots prefer a balanced four-by-three layout. */
final class DesktopSlotLayout {
  static final int CARD_WIDTH = 224, CARD_HEIGHT = 178, GAP = 8;

  static int columns(int availableWidth) {
    for (int n : new int[] {4, 3, 2, 1})
      if (n * CARD_WIDTH + (n - 1) * GAP <= availableWidth) return n;
    return 1;
  }

  static int latest(List<SlotThumbnailStore.Entry> entries) {
    int result = 0;
    long modified = Long.MIN_VALUE;
    for (var entry : entries)
      if (entry.exists && entry.modified > modified) {
        modified = entry.modified;
        result = entry.slot;
      }
    return result;
  }

  private DesktopSlotLayout() {}
}
