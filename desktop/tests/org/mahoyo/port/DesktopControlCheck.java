package org.mahoyo.port;

import android.content.Context;
import android.graphics.*;
import java.io.File;
import java.util.*;
import org.json.*;

/** Authored UI fixtures only. Reading a resource index does not start or advance the story. */
public final class DesktopControlCheck {
  public static void main(String[] args) throws Exception {
    Context.home = new File(".local/windows/control-check");
    Context.assets = new File("build/windows/assets");
    GameResources resources = new GameResources(new Context(), new File(args[0]));
    List<String> commands = new ArrayList<>();
    DesktopControls controls =
        new DesktopControls(resources, (type, payload) -> commands.add(type + ":" + payload));
    JSONObject model =
        new JSONObject()
            .put("revision", "pad-check")
            .put("profile", "text161")
            .put("initialFocus", -1)
            .put(
                "items", new JSONArray().put(item(0, false)).put(item(1, true)).put(item(2, true)));
    Scene scene = new Scene(new JSONObject().put("scene", "play").put("choice", model).toString());
    Bitmap bitmap = Bitmap.createBitmap(1280, 720, Bitmap.Config.ARGB_8888);
    Viewport viewport = new Viewport(1280, 720, 0, false, true, 1920, 1080);
    controls.state(scene);
    controls.presented(scene);
    Canvas canvas = new Canvas(bitmap);
    controls.draw(canvas, viewport);
    canvas.close();
    // A new snapshot with the same revision must retain the already rendered click targets.
    controls.state(new Scene(scene.json.toString()));
    controls.navigate(1);
    controls.accept();
    if (!commands.equals(List.of("choiceSelect:pad-check 2")))
      throw new AssertionError("Choice navigation or presented-target retention: " + commands);
    commands.clear();
    JSONObject modal =
        new JSONObject()
            .put("id", "4")
            .put("revision", 1)
            .put("phase", "ready")
            .put(
                "actions",
                new JSONArray()
                    .put(new JSONObject().put("id", "next").put("enabled", true))
                    .put(new JSONObject().put("id", "save").put("enabled", false))
                    .put(new JSONObject().put("id", "title").put("enabled", true)));
    Scene chapter =
        new Scene(new JSONObject().put("scene", "teatime").put("modal", modal).toString());
    controls.state(chapter);
    controls.presented(chapter);
    canvas = new Canvas(bitmap);
    controls.draw(canvas, viewport);
    canvas.close();
    controls.navigate(1);
    controls.accept();
    if (!commands.equals(List.of("modalAction:4 1 title")))
      throw new AssertionError("Chapter action did not skip the disabled button: " + commands);
    System.out.println(
        "Native choice/chapter controller selection, disabled actions and rendered-target retention"
            + " passed");
  }

  private static JSONObject item(int ordinal, boolean enabled) {
    return new JSONObject()
        .put("ordinal", ordinal)
        .put("label", "Option " + ordinal)
        .put("enabled", enabled);
  }
}
