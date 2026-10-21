package org.mahoyo.port;
import org.junit.Test;import static org.junit.Assert.*;
public class InstallRetryTest {
 @Test public void failedAuthenticationRetriesTheRequestedLoginEvenWhenAlreadyOwned(){for(int method:new int[]{InstallRetry.QR,InstallRetry.PASSWORD})for(String stage:new String[]{"connecting","qr","authenticating","ownership","guard","mobile","downloading"})assertEquals(method,InstallRetry.action(method,stage,"STEAM_DISCONNECTED"));}
 @Test public void importsAndVersionDenialReturnToInstallationOptions(){for(String stage:new String[]{"choose","importing","verifying","preparing","ready"})assertEquals(InstallRetry.OPTIONS,InstallRetry.action(InstallRetry.QR,stage,"bad data"));assertEquals(InstallRetry.OPTIONS,InstallRetry.action(InstallRetry.QR,"downloading","PINNED_MANIFEST_UNAVAILABLE"));assertEquals(InstallRetry.OPTIONS,InstallRetry.action(InstallRetry.OPTIONS,"connecting","STEAM_DISCONNECTED"));}
}
