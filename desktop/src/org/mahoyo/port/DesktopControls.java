package org.mahoyo.port;

import android.graphics.*;
import java.util.*;
import java.util.function.BiConsumer;
import org.json.*;

/** Native chapter gates and story choices consume background clicks just as on Android. */
final class DesktopControls {
  private static final class Hit {
    final RectF rect;
    final Runnable action;

    Hit(RectF rect, Runnable action) {
      this.rect = rect;
      this.action = action;
    }
  }

  private final ChoiceSession choices = new ChoiceSession();
  private final Map<String, Bitmap> pictures = new HashMap<>(), captions = new HashMap<>();
  private final List<Hit> hits = new ArrayList<>();
  private final GameResources resources;
  private final BiConsumer<String, String> command;
  private String language = "";
  private JSONObject modal;
  private String saving = "";

  DesktopControls(GameResources resources, BiConsumer<String, String> command) {
    this.resources = resources;
    this.command = command;
  }

  void state(Scene scene) {
    if (!language.equals(resources.language())) {
      language = resources.language();
      pictures.clear();
      captions.clear();
    }
    choices.state(ChoiceModel.parse(scene.json.optJSONObject("choice")));
    modal = scene.json.optJSONObject("modal");
  }

  void presented(Scene scene) {
    JSONObject value = scene.json.optJSONObject("choice");
    choices.presented(value == null ? "" : value.optString("revision"));
  }

  String visualKey() {
    ChoiceModel shown = choices.shown();
    return (shown == null ? "" : shown.revision)
        + "/"
        + (modal == null ? "" : modal.toString())
        + "/"
        + resources.language();
  }

  boolean visible() {
    return choices.shown() != null || modal != null && modal.optString("phase").equals("ready");
  }

  boolean active() {
    return choices.active() || modal != null;
  }

  String takeSave() {
    if (modal == null || !modal.optString("phase").equals("saving")) return null;
    String key = modal.optString("id") + " " + modal.optLong("revision");
    if (key.equals(saving)) return null;
    saving = key;
    return key + " " + modal.optString("returnAction", "save-return");
  }

  void draw(Canvas canvas, Viewport viewport) {
    hits.clear();
    ChoiceModel model = choices.shown();
    Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    if (model != null) {
      paint.setColor(0x99101e33);
      canvas.drawRect(viewport.content, paint);
      choices.blocked(false);
      if (model.profile.equals("wik164")) {
        for (ChoiceModel.Item item : model.items) {
          try {
            Bitmap image = pictures.get(item.image);
            if (image == null) {
              image = resources.file(item.image).bitmap(new BitmapFactory.Options());
              pictures.put(item.image, image);
            }
            RectF rect = viewport.transform(item.x, item.y, item.width, item.height);
            paint.setColor(0xffffffff);
            canvas.drawBitmap(image, null, rect, paint);
            hits.add(new Hit(rect, () -> select(item.ordinal)));
          } catch (Exception error) {
            throw new IllegalStateException(error);
          }
        }
      } else {
        float w = Math.min(900, viewport.content.width() * .8f),
            h = 58,
            gap = 12,
            top = viewport.content.centerY() - (model.items.size() * (h + gap) - gap) / 2;
        for (ChoiceModel.Item item : model.items) {
          RectF rect =
              new RectF(
                  viewport.content.centerX() - w / 2,
                  top,
                  viewport.content.centerX() + w / 2,
                  top + h);
          paint.setColor(item.enabled ? 0xff203751 : 0xff172230);
          canvas.drawRect(rect, paint);
          try {
            String key = item.label + "/" + w;
            Bitmap caption = captions.get(key);
            if (caption == null) {
              caption =
                  TextOverlay.captureHistory(
                      item.label, Math.round(w - 32), 24, 40, 0xffeff5ff, false);
              captions.put(key, caption);
            }
            canvas.drawBitmap(
                caption, rect.left + 16, rect.centerY() - caption.getHeight() / 2, paint);
          } catch (Exception error) {
            throw new IllegalStateException(error);
          }
          if (item.enabled) hits.add(new Hit(rect, () -> select(item.ordinal)));
          top += h + gap;
        }
      }
    }
    if (modal != null && modal.optString("phase").equals("ready")) {
      JSONArray actions = modal.optJSONArray("actions");
      if (actions != null) {
        float total = actions.length() * 150 + (actions.length() - 1) * 12,
            x = viewport.content.centerX() - total / 2,
            y = viewport.content.bottom - 60;
        for (int i = 0; i < actions.length(); i++) {
          JSONObject a = actions.getJSONObject(i);
          RectF rect = new RectF(x, y, x + 150, y + 44);
          boolean enabled = a.optBoolean("enabled");
          paint.setColor(enabled ? 0xe623374e : 0xb01b2532);
          canvas.drawRect(rect, paint);
          paint.setColor(enabled ? 0xfff0f5fc : 0xff788698);
          paint.setTextSize(20);
          String text = a.optString("label", a.optString("id"));
          canvas.drawText(
              text, rect.centerX() - paint.measureText(text) / 2, rect.centerY() + 7, paint);
          if (enabled) {
            String payload =
                modal.optString("id") + " " + modal.optLong("revision") + " " + a.optString("id");
            hits.add(new Hit(rect, () -> command.accept("modalAction", payload)));
          }
          x += 162;
        }
      }
    }
  }

  private void select(int ordinal) {
    ChoiceSession.Request request = choices.begin(ordinal);
    if (request != null) {
      command.accept("choiceSelect", request.payload());
      choices.complete(request);
    }
  }

  boolean click(float x, float y) {
    for (int i = hits.size() - 1; i >= 0; i--)
      if (hits.get(i).rect.contains(x, y)) {
        hits.get(i).action.run();
        return true;
      }
    return active();
  }
}
