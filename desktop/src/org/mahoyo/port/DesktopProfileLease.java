package org.mahoyo.port;

import java.io.*;
import java.nio.channels.*;
import java.nio.file.StandardOpenOption;

/** Prevents two players, or an offline sync and a player, from writing the same profile. */
final class DesktopProfileLease implements AutoCloseable {
  private final FileChannel channel;
  private final FileLock lock;

  private DesktopProfileLease(FileChannel channel, FileLock lock) {
    this.channel = channel;
    this.lock = lock;
  }

  static DesktopProfileLease acquire(File profile) throws IOException {
    profile.mkdirs();
    FileChannel channel =
        FileChannel.open(
            new File(profile, "runtime.lock").toPath(),
            StandardOpenOption.CREATE,
            StandardOpenOption.WRITE);
    try {
      FileLock lock = channel.tryLock();
      if (lock == null) {
        channel.close();
        return null;
      }
      return new DesktopProfileLease(channel, lock);
    } catch (OverlappingFileLockException busy) {
      channel.close();
      return null;
    } catch (IOException failure) {
      channel.close();
      throw failure;
    }
  }

  @Override
  public void close() throws IOException {
    lock.release();
    channel.close();
  }
}
