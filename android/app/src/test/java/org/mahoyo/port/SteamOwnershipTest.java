package org.mahoyo.port;
import in.dragonbra.javasteam.enums.ELicenseFlags;import java.util.*;import org.junit.Test;import static org.junit.Assert.*;
public class SteamOwnershipTest {
 @Test public void ownPermanentLicenseAndUnsignedAccountsAreSupported(){assertTrue(SteamOwnership.accessible(123,123,Collections.emptySet()));assertTrue(SteamOwnership.accessible(0,123,Collections.emptySet()));assertTrue(SteamOwnership.accessible(-1,4294967295L,Collections.emptySet()));}
 @Test public void familyAccessCanActivateOfflinePlayback(){
  assertTrue(SteamOwnership.accessible(456,123,Collections.emptySet()));
  assertTrue(SteamOwnership.accessible(123,123,EnumSet.of(ELicenseFlags.Borrowed)));
  assertTrue(SteamOwnership.accessible(456,123,EnumSet.of(ELicenseFlags.Borrowed,ELicenseFlags.NonPermanent)));
  assertTrue(SteamOwnership.accessible(456,123,EnumSet.of(ELicenseFlags.NonPermanent)));
  assertTrue(SteamOwnership.accessible(-1,123,Collections.emptySet()));
 }
 @Test public void invalidAndUnsharedTemporaryLicensesRemainRejected(){
  assertFalse(SteamOwnership.accessible(123,123,EnumSet.of(ELicenseFlags.NonPermanent)));
  for(ELicenseFlags flag:SteamOwnership.REJECTED){
   assertFalse(flag.name(),SteamOwnership.accessible(123,123,EnumSet.of(flag)));
   assertFalse(flag.name(),SteamOwnership.accessible(456,123,EnumSet.of(ELicenseFlags.Borrowed,flag)));
  }
 }
}
