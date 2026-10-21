package org.mahoyo.port;
import in.dragonbra.javasteam.enums.ELicenseFlags;import java.util.*;import org.junit.Test;import static org.junit.Assert.*;
public class SteamOwnershipTest {
 @Test public void ownPermanentLicenseAndUnsignedAccountsAreSupported(){assertTrue(SteamOwnership.permanent(123,123,Collections.emptySet()));assertTrue(SteamOwnership.permanent(0,123,Collections.emptySet()));assertTrue(SteamOwnership.permanent(-1,4294967295L,Collections.emptySet()));}
 @Test public void sharedAndTemporaryAccessDoNotGrantOfflineActivation(){assertFalse(SteamOwnership.permanent(456,123,Collections.emptySet()));for(ELicenseFlags flag:SteamOwnership.REJECTED)assertFalse(flag.name(),SteamOwnership.permanent(123,123,EnumSet.of(flag)));}
}
