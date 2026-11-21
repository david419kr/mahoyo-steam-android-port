package org.mahoyo.port;

import com.sun.net.httpserver.*;
import java.io.*;
import java.net.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.*;
import org.json.*;

/** Coordinates the standalone synchronizer with the running player's in-memory progress. */
final class DesktopSaveSync implements AutoCloseable {
  interface Host {
    void begin() throws Exception;

    void imported() throws Exception;

    void end();
  }

  private final Host host;
  private final File profile, saves, thumbs, endpoint;
  private final HttpServer server;
  private final ExecutorService worker;
  private final ScheduledExecutorService timer;
  private final String token = UUID.randomUUID().toString();
  private volatile Session session;

  private static final class Session {
    final String id = UUID.randomUUID().toString();
    final File root, exported, uploaded;
    String original;
    boolean committed;

    Session(File profile) throws IOException {
      File cache = new File(profile, "cache/save-sync");
      cache.mkdirs();
      root = Files.createTempDirectory(cache.toPath(), "session-").toFile();
      exported = new File(root, "export.zip");
      uploaded = new File(root, "import.zip");
    }
  }

  DesktopSaveSync(File profile, Host host) throws Exception {
    this.profile = profile;
    this.host = host;
    saves = new File(profile, "savedata");
    thumbs = new File(profile, "slot-thumbnails");
    endpoint = new File(profile, "save-sync.endpoint");
    worker =
        Executors.newSingleThreadExecutor(
            r -> {
              Thread t = new Thread(r, "Mahoyo sync server");
              t.setDaemon(true);
              return t;
            });
    timer =
        Executors.newSingleThreadScheduledExecutor(
            r -> {
              Thread t = new Thread(r, "Mahoyo sync expiry");
              t.setDaemon(true);
              return t;
            });
    server = HttpServer.create(new InetSocketAddress(InetAddress.getByName("127.0.0.1"), 0), 2);
    server.setExecutor(worker);
    server.createContext("/", this::request);
    server.start();
    JSONObject record =
        new JSONObject()
            .put("protocol", 1)
            .put("pid", ProcessHandle.current().pid())
            .put("port", server.getAddress().getPort())
            .put("token", token);
    File staged = new File(endpoint + ".new");
    Files.writeString(staged.toPath(), record.toString());
    Files.move(staged.toPath(), endpoint.toPath(), StandardCopyOption.REPLACE_EXISTING);
  }

  private void request(HttpExchange exchange) throws IOException {
    try {
      String supplied = exchange.getRequestHeaders().getFirst("X-Mahoyo-Sync");
      if (supplied == null
          || !MessageDigest.isEqual(
              token.getBytes(java.nio.charset.StandardCharsets.UTF_8),
              supplied.getBytes(java.nio.charset.StandardCharsets.UTF_8))) {
        exchange.sendResponseHeaders(403, -1);
        return;
      }
      String path = exchange.getRequestURI().getPath();
      if (path.equals("/begin")) {
        if (session != null) throw new IOException("Another synchronization is in progress");
        host.begin();
        Session current = null;
        try {
          current = new Session(profile);
          try (SaveArchive.Archive snapshot = SaveSyncMerge.snapshot(saves, thumbs, current.root)) {
            current.original = SaveSyncMerge.fingerprint(snapshot);
            SaveArchive.write(snapshot, new FileOutputStream(current.exported), "windows-sync-1");
          }
          session = current;
          String id = current.id;
          timer.schedule(
              () -> {
                Session s = session;
                if (s != null && s.id.equals(id)) {
                  try {
                    worker.submit(() -> finish(s));
                  } catch (RejectedExecutionException ignored) {
                  }
                }
              },
              5,
              TimeUnit.MINUTES);
          json(exchange, 200, new JSONObject().put("session", id));
          return;
        } catch (Exception failure) {
          host.end();
          if (current != null) cleanup(current);
          throw failure;
        }
      }
      String[] parts = path.split("/");
      Session current = session;
      if (parts.length != 3 || current == null || !parts[1].equals(current.id))
        throw new IOException("Save synchronization session has expired");
      if (parts[2].equals("export")) {
        exchange.getResponseHeaders().set("Content-Type", "application/zip");
        exchange.sendResponseHeaders(200, current.exported.length());
        Files.copy(current.exported.toPath(), exchange.getResponseBody());
        return;
      }
      if (parts[2].equals("import") && exchange.getRequestMethod().equals("POST")) {
        try (OutputStream out = new FileOutputStream(current.uploaded);
            InputStream in = exchange.getRequestBody()) {
          byte[] block = new byte[65536];
          long total = 0;
          int n;
          while ((n = in.read(block)) != -1) {
            total += n;
            if (total > SaveArchive.MAX_TOTAL + 16 * 1024 * 1024)
              throw new IOException("Save ZIP exceeds transfer limit");
            out.write(block, 0, n);
          }
        }
        json(exchange, 200, new JSONObject().put("uploaded", true));
        return;
      }
      if (parts[2].equals("commit")) {
        if (current.committed) throw new IOException("Synchronization was already committed");
        try (SaveArchive.Archive existing = SaveSyncMerge.snapshot(saves, thumbs, current.root);
            SaveArchive.Archive incoming =
                SaveArchive.read(new FileInputStream(current.uploaded), current.root, true)) {
          if (!SaveSyncMerge.fingerprint(existing).equals(current.original))
            throw new IOException("Save data changed; retry synchronization");
          SaveArchive.Plan plan = SaveArchive.exactPlan(incoming, saves, thumbs);
          if (!plan.replacements.isEmpty()) {
            File backupDir = new File(profile, "save-sync-backups");
            backupDir.mkdirs();
            Files.copy(
                current.exported.toPath(),
                new File(backupDir, System.currentTimeMillis() + "-" + current.id + ".zip")
                    .toPath());
            SaveArchive.apply(plan, new File(profile, "save-transfer"), true, host::imported);
          }
          current.committed = true;
          json(exchange, 200, new JSONObject().put("changed", plan.replacements.size()));
          return;
        }
      }
      if (parts[2].equals("verify")) {
        try (SaveArchive.Archive snapshot = SaveSyncMerge.snapshot(saves, thumbs, current.root)) {
          json(
              exchange,
              200,
              new JSONObject().put("fingerprint", SaveSyncMerge.fingerprint(snapshot)));
        }
        return;
      }
      if (parts[2].equals("end")) {
        finish(current);
        json(exchange, 200, new JSONObject().put("ended", true));
        return;
      }
      throw new IOException("Unknown save synchronization operation");
    } catch (Exception failure) {
      json(
          exchange,
          409,
          new JSONObject()
              .put(
                  "error",
                  failure.getMessage() == null
                      ? failure.getClass().getSimpleName()
                      : failure.getMessage()));
    } finally {
      exchange.close();
    }
  }

  private static void json(HttpExchange exchange, int code, JSONObject json) throws IOException {
    byte[] data = json.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);
    exchange.getResponseHeaders().set("Content-Type", "application/json; charset=utf-8");
    exchange.sendResponseHeaders(code, data.length);
    exchange.getResponseBody().write(data);
  }

  private void finish(Session current) {
    if (session != current) return;
    session = null;
    try {
      host.end();
    } finally {
      cleanup(current);
    }
  }

  private static void cleanup(Session session) {
    session.exported.delete();
    session.uploaded.delete();
    session.root.delete();
  }

  @Override
  public void close() {
    server.stop(0);
    worker.shutdownNow();
    timer.shutdownNow();
    try {
      worker.awaitTermination(30, TimeUnit.SECONDS);
    } catch (InterruptedException interrupted) {
      Thread.currentThread().interrupt();
    }
    Session s = session;
    session = null;
    if (s != null) cleanup(s);
    endpoint.delete();
  }
}
