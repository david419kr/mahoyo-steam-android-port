package org.mahoyo.port;

import java.io.*;
import java.net.*;
import java.net.http.*;
import java.nio.file.*;
import java.util.*;
import org.json.*;

/** Exercises the running player's local endpoint without touching the user's profile. */
public final class DesktopSyncCheck {
  public static void main(String[] args) throws Exception {
    File profile = Files.createTempDirectory(Path.of(".local/windows"), "sync-check-").toFile();
    int[] callbacks = {0, 0, 0};
    try (DesktopProfileLease lease = DesktopProfileLease.acquire(profile);
        DesktopSaveSync server =
            new DesktopSaveSync(
                profile,
                new DesktopSaveSync.Host() {
                  public void begin() {
                    callbacks[0]++;
                  }

                  public void imported() {
                    callbacks[1]++;
                  }

                  public void end() {
                    callbacks[2]++;
                  }
                });
        HttpClient http =
            HttpClient.newBuilder()
                .version(HttpClient.Version.HTTP_1_1)
                .connectTimeout(java.time.Duration.ofSeconds(5))
                .build()) {
      if (DesktopProfileLease.acquire(profile) != null)
        throw new AssertionError("Second writer was allowed");
      JSONObject endpoint =
          new JSONObject(Files.readString(new File(profile, "save-sync.endpoint").toPath()));
      String base = "http://127.0.0.1:" + endpoint.getInt("port"),
          token = endpoint.getString("token");
      var denied =
          http.send(
              HttpRequest.newBuilder(URI.create(base + "/begin"))
                  .POST(HttpRequest.BodyPublishers.noBody())
                  .build(),
              HttpResponse.BodyHandlers.ofString());
      if (denied.statusCode() != 403) throw new AssertionError("Missing token allowed");
      String session =
          new JSONObject(request(http, base + "/begin", token, null)).getString("session");
      Path exported = profile.toPath().resolve("export.zip");
      http.send(
          HttpRequest.newBuilder(URI.create(base + "/" + session + "/export"))
              .header("X-Mahoyo-Sync", token)
              .build(),
          HttpResponse.BodyHandlers.ofFile(exported));
      try (SaveArchive.Archive empty =
          SaveArchive.read(Files.newInputStream(exported), profile, true)) {
        if (!empty.files.isEmpty()) throw new AssertionError();
      }
      File source = new File(profile, "fixture");
      source.mkdirs();
      byte[] save = save(17);
      Files.write(new File(source, "slot1.whs").toPath(), save);
      File incoming = new File(profile, "incoming.zip");
      String expected;
      try (SaveArchive.Archive fixture =
          SaveSyncMerge.snapshot(source, new File(profile, "fixture-thumbs"), profile)) {
        expected = SaveSyncMerge.fingerprint(fixture);
        SaveArchive.write(fixture, new FileOutputStream(incoming), "test");
      }
      request(http, base + "/" + session + "/import", token, incoming);
      request(http, base + "/" + session + "/commit", token, null);
      String fingerprint =
          new JSONObject(request(http, base + "/" + session + "/verify", token, null))
              .getString("fingerprint");
      if (!fingerprint.equals(expected)) throw new AssertionError("Live commit differs");
      request(http, base + "/" + session + "/end", token, null);
      if (!Arrays.equals(callbacks, new int[] {1, 1, 1})
          || !Arrays.equals(
              save, Files.readAllBytes(new File(profile, "savedata/slot1.whs").toPath())))
        throw new AssertionError("Live callbacks/save mismatch");
      System.out.println(
          "Live sync token, writer lock, empty export, commit, verification and resume passed");
    }
  }

  private static String request(HttpClient http, String url, String token, File file)
      throws Exception {
    var builder =
        HttpRequest.newBuilder(URI.create(url))
            .timeout(java.time.Duration.ofSeconds(8))
            .header("X-Mahoyo-Sync", token);
    builder.POST(
        file == null
            ? HttpRequest.BodyPublishers.noBody()
            : HttpRequest.BodyPublishers.ofFile(file.toPath()));
    var response = http.send(builder.build(), HttpResponse.BodyHandlers.ofString());
    if (response.statusCode() != 200) throw new AssertionError(response.body());
    return response.body();
  }

  private static byte[] save(int marker) {
    var b = java.nio.ByteBuffer.allocate(52).order(java.nio.ByteOrder.LITTLE_ENDIAN);
    b.put("WOHSAVE2".getBytes(java.nio.charset.StandardCharsets.US_ASCII))
        .putInt(36)
        .putInt(0)
        .putInt(4)
        .putInt(marker);
    for (int i = 0; i < 7; i++) b.putInt(0);
    var crc = new java.util.zip.CRC32();
    crc.update(b.array(), 16, 36);
    b.putInt(12, (int) crc.getValue());
    return b.array();
  }
}
