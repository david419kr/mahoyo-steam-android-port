package org.mahoyo.port;

import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;
import java.net.http.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.time.Duration;
import java.util.*;
import java.util.List;
import java.util.concurrent.*;
import java.util.function.Consumer;
import javax.swing.*;
import org.json.*;

/** One-click, local ADB synchronization. Transfers saved slots and common progress only. */
public final class SaveSyncMain {
  static final String PACKAGE = "org.mahoyo.port.unofficial";
  private static final String TUNNEL = "127.0.0.1:17555";
  private static final boolean KO = Locale.getDefault().getLanguage().equals("ko");

  private static String text(String en, String ko) {
    return KO ? ko : en;
  }

  public static void main(String[] args) {
    if (args.length == 2 && args[0].equals("--serial")) {
      try {
        sync(
            new File(
                System.getProperty(
                    "mahoyo.home", System.getenv("LOCALAPPDATA") + "/MahoyoUnofficial")),
            args[1],
            System.out::println,
            null);
      } catch (Exception e) {
        e.printStackTrace();
        System.exit(1);
      }
      return;
    }
    SwingUtilities.invokeLater(
        () -> {
          JFrame frame = new JFrame(text("Mahoyo · Save synchronization", "마법사의 밤 · 세이브 동기화"));
          frame.setDefaultCloseOperation(WindowConstants.DO_NOTHING_ON_CLOSE);
          JTextArea status = new JTextArea(11, 55);
          status.setEditable(false);
          status.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 15));
          status.setMargin(new Insets(18, 18, 18, 18));
          status.setLineWrap(true);
          status.setWrapStyleWord(true);
          JProgressBar progress = new JProgressBar();
          progress.setIndeterminate(true);
          JButton close = new JButton(text("Close", "닫기"));
          close.setEnabled(false);
          close.addActionListener(e -> frame.dispose());
          JPanel bottom = new JPanel(new BorderLayout(12, 0));
          bottom.setBorder(BorderFactory.createEmptyBorder(0, 18, 14, 18));
          bottom.add(progress);
          bottom.add(close, BorderLayout.EAST);
          frame.add(new JScrollPane(status));
          frame.add(bottom, BorderLayout.SOUTH);
          frame.pack();
          frame.setLocationRelativeTo(null);
          frame.setVisible(true);
          new SwingWorker<Void, String>() {
            protected Void doInBackground() throws Exception {
              sync(
                  new File(
                      System.getProperty(
                          "mahoyo.home", System.getenv("LOCALAPPDATA") + "/MahoyoUnofficial")),
                  null,
                  this::publish,
                  frame);
              return null;
            }

            protected void process(List<String> lines) {
              for (String line : lines) status.append(line + "\n");
            }

            protected void done() {
              progress.setIndeterminate(false);
              close.setEnabled(true);
              frame.setDefaultCloseOperation(WindowConstants.DISPOSE_ON_CLOSE);
              try {
                get();
                status.append(text("\nSynchronization complete.", "\n동기화를 완료했습니다."));
              } catch (Exception e) {
                Throwable cause = e.getCause() == null ? e : e.getCause();
                status.append(
                    "\n" + text("Synchronization stopped: ", "동기화 중단: ") + cause.getMessage());
              }
            }
          }.execute();
        });
  }

  static void sync(File profile, String serial, Consumer<String> log, JFrame owner)
      throws Exception {
    profile.mkdirs();
    File cache = new File(profile, "cache");
    cache.mkdirs();
    Path work = Files.createTempDirectory(cache.toPath(), "sync-client-");
    try {
      log.accept(text("Checking USB devices and reverse tunnel…", "USB 기기와 리버스 터널을 확인합니다…"));
      Adb adb = new Adb(findAdb(), work);
      try {
        adb.run(12, "connect", TUNNEL);
      } catch (IOException ignored) {
      }
      try {
        adb.run(8, "-s", TUNNEL, "get-state");
      } catch (IOException ignored) {
      }
      String devices = adb.run(12, "devices", "-l");
      if (serial == null) {
        List<String> supported = new ArrayList<>();
        for (String line : devices.split("\\R")) {
          String[] fields = line.trim().split("\\s+");
          if (fields.length < 2 || !fields[1].equals("device")) continue;
          String installed =
              adb.run(15, "-s", fields[0], "shell", "pm", "path", "--user", "0", PACKAGE);
          if (installed.contains("package:")) supported.add(fields[0]);
        }
        if (supported.isEmpty())
          throw new IOException(
              text(
                  "No authorized Android device with the game installed was found. Enable USB"
                      + " debugging and approve this PC.",
                  "게임이 설치된 ADB 승인 기기를 찾지 못했습니다. USB 디버깅과 PC 연결 승인을 확인해 주세요."));
        if (supported.size() == 1) serial = supported.getFirst();
        else {
          if (owner == null) throw new IOException("Multiple devices; use --serial");
          Object[] choice = {null};
          List<String> options = supported;
          SwingUtilities.invokeAndWait(
              () ->
                  choice[0] =
                      JOptionPane.showInputDialog(
                          owner,
                          text("Choose the device to synchronize", "동기화할 기기를 선택해 주세요"),
                          "ADB",
                          JOptionPane.QUESTION_MESSAGE,
                          null,
                          options.toArray(),
                          options.contains(TUNNEL) ? TUNNEL : options.getFirst()));
          if (choice[0] == null) throw new IOException(text("Canceled", "취소했습니다"));
          serial = choice[0].toString();
        }
      }
      log.accept(text("Device: ", "기기: ") + serial);
      Phone phone = new Phone(adb, serial);
      String version = phone.raw("version", null);
      if (!version.contains("protocol=1")) {
        File apk =
            new File(System.getProperty("mahoyo.package", "."), "android/mahoyo-save-sync.apk");
        if (owner == null || !apk.isFile())
          throw new IOException(
              text(
                  "Update the Android app with the save-sync APK included in this package, then"
                      + " retry.",
                  "동봉된 세이브 동기화 지원 APK로 Android 앱을 업데이트한 뒤 다시 실행해 주세요."));
        int[] answer = {JOptionPane.NO_OPTION};
        SwingUtilities.invokeAndWait(
            () ->
                answer[0] =
                    JOptionPane.showConfirmDialog(
                        owner,
                        text(
                            "The Android app needs a save-sync update. Updating closes the running"
                                + " game. Save your progress first. Install the included APK now?",
                            "Android 앱에 동기화 지원 업데이트가 필요합니다. 설치하면 실행 중인 게임이 종료됩니다. 진행 상황을 먼저 저장해"
                                + " 주세요. 동봉 APK를 설치할까요?"),
                        "Mahoyo",
                        JOptionPane.YES_NO_OPTION));
        if (answer[0] != JOptionPane.YES_OPTION)
          throw new IOException(text("Update canceled", "업데이트를 취소했습니다"));
        adb.run(180, "-s", serial, "install", "-r", apk.getAbsolutePath());
        version = phone.raw("version", null);
        if (!version.contains("protocol=1"))
          throw new IOException("Android save-sync protocol is unavailable");
      }
      try (Pc pc = new Pc(profile, work)) {
        log.accept(text("Preparing save snapshots…", "저장 데이터 사본을 준비합니다…"));
        try {
          phone.begin();
          File phoneZip = work.resolve("phone.zip").toFile();
          phone.exportTo(phoneZip);
          File pcZip = work.resolve("pc.zip").toFile();
          pc.exportTo(pcZip);
          try (SaveArchive.Archive a =
                  SaveArchive.read(new FileInputStream(pcZip), work.toFile(), true);
              SaveArchive.Archive b =
                  SaveArchive.read(new FileInputStream(phoneZip), work.toFile(), true);
              SaveArchive.Archive merged = SaveSyncMerge.merge(a, b, work.toFile())) {
            String expected = SaveSyncMerge.fingerprint(merged);
            boolean pcChanged = !expected.equals(SaveSyncMerge.fingerprint(a)),
                phoneChanged = !expected.equals(SaveSyncMerge.fingerprint(b));
            log.accept(
                text(
                    "Keeping the newest version of each slot; merging shared progress.",
                    "슬롯별 최신 저장을 선택하고 공통 진행 기록을 합칩니다."));
            File mergedZip = work.resolve("merged.zip").toFile();
            SaveArchive.write(merged, new FileOutputStream(mergedZip), "windows-sync-1");
            // Preserve both originals locally before either side is changed, including partial
            // failures.
            if (pcChanged || phoneChanged) {
              File backup =
                  new File(
                      profile,
                      "save-sync-backups/" + System.currentTimeMillis() + "-" + UUID.randomUUID());
              backup.mkdirs();
              Files.copy(pcZip.toPath(), new File(backup, "windows.zip").toPath());
              Files.copy(phoneZip.toPath(), new File(backup, "android.zip").toPath());
              log.accept(text("Originals backed up: ", "원본 백업: ") + backup);
            }
            if (phoneChanged) {
              phone.importFrom(mergedZip);
              phone.call("commit");
            }
            if (pcChanged) pc.apply(merged, mergedZip);
            if (!phone.call("verify").contains("fingerprint=" + expected)
                || !pc.fingerprint().equals(expected))
              throw new IOException(
                  text(
                      "Verification did not match. Original backups were preserved; retry"
                          + " synchronization.",
                      "동기화 결과 검증이 일치하지 않습니다. 원본 백업은 보존했습니다. 다시 동기화해 주세요."));
            log.accept(
                text("Verified slots: ", "검증된 슬롯: ")
                    + merged.slots().size()
                    + text(" · shared progress included", " · 공통 진행 기록 포함"));
          }
        } finally {
          phone.close();
        }
      }
    } finally {
      try (var paths = Files.walk(work)) {
        for (Path p : paths.sorted(Comparator.reverseOrder()).toList()) Files.deleteIfExists(p);
      }
    }
  }

  private static String findAdb() throws IOException {
    List<String> candidates = new ArrayList<>();
    String explicit = System.getProperty("mahoyo.adb");
    if (explicit != null) candidates.add(explicit);
    for (String variable : List.of("ANDROID_HOME", "ANDROID_SDK_ROOT")) {
      String value = System.getenv(variable);
      if (value != null) candidates.add(value + "/platform-tools/adb.exe");
    }
    candidates.add(System.getenv("LOCALAPPDATA") + "/Android/Sdk/platform-tools/adb.exe");
    candidates.add("C:/Program Files/platform-tools/adb.exe");
    candidates.add("C:/platform-tools/adb.exe");
    for (String directory : System.getenv().getOrDefault("PATH", "").split(File.pathSeparator))
      candidates.add(directory + "/adb.exe");
    for (String candidate : candidates) if (new File(candidate).isFile()) return candidate;
    throw new IOException(
        text(
            "ADB was not found. Install Android SDK Platform-Tools from"
                + " developer.android.com/tools/releases/platform-tools.",
            "ADB를 찾지 못했습니다. Android SDK Platform-Tools를 설치해 주세요:"
                + " developer.android.com/tools/releases/platform-tools"));
  }

  private static final class Adb {
    final String exe;
    final Path work;

    Adb(String exe, Path work) {
      this.exe = exe;
      this.work = work;
    }

    String run(int timeout, String... args) throws IOException {
      Path output = Files.createTempFile(work, "adb-", ".txt");
      try {
        execute(timeout, output.toFile(), null, args);
        return Files.readString(output, StandardCharsets.UTF_8).trim();
      } finally {
        Files.deleteIfExists(output);
      }
    }

    void execute(int timeout, File output, File input, String... args) throws IOException {
      ArrayList<String> command = new ArrayList<>();
      command.add(exe);
      Collections.addAll(command, args);
      Path err = Files.createTempFile(work, "adb-", ".err");
      Process process = null;
      try {
        ProcessBuilder builder = new ProcessBuilder(command).redirectError(err.toFile());
        if (output != null) builder.redirectOutput(output);
        else builder.redirectOutput(ProcessBuilder.Redirect.DISCARD);
        if (input != null) builder.redirectInput(input);
        process = builder.start();
        if (input == null) process.getOutputStream().close();
        if (!process.waitFor(timeout, TimeUnit.SECONDS)) {
          process.destroyForcibly();
          throw new IOException("ADB operation timed out");
        }
        if (process.exitValue() != 0)
          throw new IOException("ADB: " + Files.readString(err, StandardCharsets.UTF_8).trim());
      } catch (InterruptedException interrupted) {
        if (process != null) process.destroyForcibly();
        Thread.currentThread().interrupt();
        throw new IOException(interrupted);
      } finally {
        Files.deleteIfExists(err);
      }
    }
  }

  private static final class Phone implements AutoCloseable {
    final Adb adb;
    final String serial;
    String session;

    Phone(Adb adb, String serial) {
      this.adb = adb;
      this.serial = serial;
    }

    String raw(String method, String token) throws IOException {
      ArrayList<String> args =
          new ArrayList<>(
              List.of(
                  "-s",
                  serial,
                  "shell",
                  "content",
                  "call",
                  "--user",
                  "0",
                  "--uri",
                  "content://" + PACKAGE + ".savesync",
                  "--method",
                  method));
      if (token != null) {
        args.add("--arg");
        args.add(token);
      }
      return adb.run(90, args.toArray(String[]::new));
    }

    String call(String method) throws IOException {
      String result = raw(method, session);
      if (!result.startsWith("Result: Bundle[") || result.contains("error="))
        throw new IOException("Android: " + result);
      return result;
    }

    void begin() throws IOException {
      String result = call("begin");
      var matcher = java.util.regex.Pattern.compile("session=([a-f0-9-]{36})").matcher(result);
      if (!matcher.find()) throw new IOException("Android: " + result);
      session = matcher.group(1);
    }

    String uri(String operation) {
      return "content://" + PACKAGE + ".savesync/" + session + "/" + operation;
    }

    void exportTo(File file) throws IOException {
      adb.execute(
          90,
          file,
          null,
          "-s",
          serial,
          "exec-out",
          "content",
          "read",
          "--user",
          "0",
          "--uri",
          uri("export"));
    }

    void importFrom(File file) throws IOException {
      adb.execute(
          90,
          null,
          file,
          "-s",
          serial,
          "exec-in",
          "content",
          "write",
          "--user",
          "0",
          "--uri",
          uri("import"));
    }

    public void close() {
      if (session != null)
        try {
          call("end");
        } catch (IOException ignored) {
        }
      session = null;
    }
  }

  private static final class Pc implements AutoCloseable {
    final File profile, saves, thumbs;
    final Path work;
    final DesktopProfileLease lease;
    final JSONObject endpoint;
    String session;
    final HttpClient http =
        HttpClient.newBuilder()
            .version(HttpClient.Version.HTTP_1_1)
            .connectTimeout(Duration.ofSeconds(5))
            .build();

    Pc(File profile, Path work) throws Exception {
      this.profile = profile;
      this.work = work;
      saves = new File(profile, "savedata");
      thumbs = new File(profile, "slot-thumbnails");
      lease = DesktopProfileLease.acquire(profile);
      if (lease != null) {
        endpoint = null;
        SaveArchive.recover(saves, thumbs, new File(profile, "save-transfer"));
      } else {
        File file = new File(profile, "save-sync.endpoint");
        if (!file.isFile())
          throw new IOException("The Windows player is still starting. Retry shortly.");
        endpoint = new JSONObject(Files.readString(file.toPath()));
        if (endpoint.optInt("protocol") != 1
            || ProcessHandle.of(endpoint.getLong("pid")).filter(ProcessHandle::isAlive).isEmpty())
          throw new IOException("The Windows player is not ready for synchronization");
        session = new JSONObject(request("begin", null).body()).getString("session");
      }
    }

    HttpResponse<String> request(String operation, File body) throws Exception {
      int port = endpoint.getInt("port");
      if (port < 1 || port > 65535) throw new IOException("Invalid local save-sync port");
      String path = session == null ? "/" + operation : "/" + session + "/" + operation;
      var builder =
          HttpRequest.newBuilder(URI.create("http://127.0.0.1:" + port + path))
              .timeout(Duration.ofSeconds(90))
              .header("X-Mahoyo-Sync", endpoint.getString("token"));
      if (body != null) builder.POST(HttpRequest.BodyPublishers.ofFile(body.toPath()));
      else builder.POST(HttpRequest.BodyPublishers.noBody());
      HttpResponse<String> result =
          http.send(builder.build(), HttpResponse.BodyHandlers.ofString());
      if (result.statusCode() != 200) throw new IOException("Windows: " + result.body());
      return result;
    }

    void exportTo(File file) throws Exception {
      if (endpoint == null) {
        try (SaveArchive.Archive snapshot = SaveSyncMerge.snapshot(saves, thumbs, work.toFile())) {
          SaveArchive.write(snapshot, new FileOutputStream(file), "windows-sync-1");
        }
      } else {
        HttpRequest request =
            HttpRequest.newBuilder(
                    URI.create(
                        "http://127.0.0.1:" + endpoint.getInt("port") + "/" + session + "/export"))
                .timeout(Duration.ofSeconds(90))
                .header("X-Mahoyo-Sync", endpoint.getString("token"))
                .build();
        var result = http.send(request, HttpResponse.BodyHandlers.ofFile(file.toPath()));
        if (result.statusCode() != 200)
          throw new IOException("Cannot read the running Windows player's saves");
      }
    }

    void apply(SaveArchive.Archive merged, File zip) throws Exception {
      if (endpoint == null)
        SaveArchive.apply(
            SaveArchive.exactPlan(merged, saves, thumbs),
            new File(profile, "save-transfer"),
            true,
            () -> {});
      else {
        request("import", zip);
        request("commit", null);
      }
    }

    String fingerprint() throws Exception {
      if (endpoint != null)
        return new JSONObject(request("verify", null).body()).getString("fingerprint");
      try (SaveArchive.Archive snapshot = SaveSyncMerge.snapshot(saves, thumbs, work.toFile())) {
        return SaveSyncMerge.fingerprint(snapshot);
      }
    }

    public void close() throws Exception {
      try {
        if (session != null) request("end", null);
      } finally {
        http.close();
        if (lease != null) lease.close();
      }
    }
  }
}
