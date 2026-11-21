package org.mahoyo.port;

import org.json.*;

/** Authored terminal DTOs only; no story data or user saves. */
public final class DesktopMainHaltCheck {
  private static JSONObject end() {
    return new JSONObject()
        .put("scene", "play")
        .put("waiting", "end")
        .put("module", "authored.chs")
        .put("version", 20)
        .put("instructions", 42)
        .put("modulePc", 64);
  }

  private static void check(boolean value, String message) {
    if (!value) throw new AssertionError(message);
  }

  public static void main(String[] args) {
    check(DesktopWindow.normalMainHalt(end()), "normal termination must return");
    for (String key :
        new String[] {"movie", "transition", "choice", "modal", "archive", "gallery", "musicRoom"})
      check(
          !DesktopWindow.normalMainHalt(end().put(key, new JSONObject())),
          key + " must finish first");
    for (String wait : new String[] {"input", "voice", "transitionReady", "movie", "error"})
      check(!DesktopWindow.normalMainHalt(end().put("waiting", wait)), "nonterminal " + wait);
    check(
        !DesktopWindow.normalMainHalt(end().put("error", "authored error")),
        "error is not an ending");
    check(
        !DesktopWindow.normalMainHalt(end().put("scene", "archive")),
        "replay keeps its Archive return");
    check(!DesktopWindow.normalMainHalt(end().put("module", "")), "empty module is not an ending");
    for (String key : new String[] {"active", "readyPending", "request", "jumpRelease"}) {
      JSONObject region =
          new JSONObject()
              .put(key, key.equals("active") || key.equals("readyPending") ? true : "1");
      check(
          !DesktopWindow.normalMainHalt(end().put("inputRegion", region)), "pending input " + key);
    }
    MainHaltReturn gate = new MainHaltReturn();
    Object scene = new Object();
    gate.observe(scene);
    check(
        gate.claim(scene, false, 20, "authored.chs", 42, 64) == null,
        "uncommitted/media/UI-blocked ending cannot return");
    MainHaltReturn.Ticket ticket = gate.claim(scene, true, 20, "authored.chs", 42, 64);
    check(
        !gate.consume(ticket, true, 21, "authored.chs", 42, 64),
        "stale engine version cannot return");
    ticket = gate.claim(scene, true, 20, "authored.chs", 42, 64);
    check(
        gate.consume(ticket, true, 20, "authored.chs", 42, 64),
        "committed unchanged ending returns");
    check(
        gate.claim(scene, true, 20, "authored.chs", 42, 64) == null,
        "same ending returns only once");
    gate.observe(new Object());
    check(
        gate.claim(scene, true, 20, "authored.chs", 42, 64) == null,
        "old scene callback cannot return");
    System.out.println(
        "Normal end, media/UI/input/error/replay guards, fresh engine identity and one-shot title"
            + " return passed");
  }
}
