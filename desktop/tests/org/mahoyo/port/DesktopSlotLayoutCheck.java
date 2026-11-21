package org.mahoyo.port;

import java.util.*;

public final class DesktopSlotLayoutCheck {
  public static void main(String[] args) {
    List<SlotThumbnailStore.Entry> entries = new ArrayList<>();
    for (int slot = 1; slot <= 12; slot++)
      entries.add(
          new SlotThumbnailStore.Entry(
              slot, true, slot == 11 ? 10000 : slot, 52, "authored", null));
    if (DesktopSlotLayout.latest(entries) != 11)
      throw new AssertionError("Newest save is not selected");
    entries.set(11, new SlotThumbnailStore.Entry(12, false, 999999, 0, "", null));
    if (DesktopSlotLayout.latest(entries) != 11)
      throw new AssertionError("An empty slot displaced the latest save");
    if (DesktopSlotLayout.latest(List.of()) != 0)
      throw new AssertionError("Empty save list has a latest slot");
    for (int width : new int[] {300, 600, 800, 1000, 1500, 3000}) {
      int columns = DesktopSlotLayout.columns(width);
      if (12 % columns != 0
          || columns * DesktopSlotLayout.CARD_WIDTH + (columns - 1) * DesktopSlotLayout.GAP > width)
        throw new AssertionError("Unbalanced/overflowing save columns");
    }
    if (DesktopSlotLayout.columns(1500) != 4 || DesktopSlotLayout.columns(1000) != 4)
      throw new AssertionError("Large/small screens do not adapt the full save grid");
    System.out.println(
        "Newest-save selection, empty-slot handling, balanced responsive columns and card-width"
            + " bounds passed");
  }
}
