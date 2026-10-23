package org.mahoyo.port;
import in.dragonbra.javasteam.enums.ELicenseFlags;
import java.util.*;
/** Accept Steam-provided family access; invalid licenses remain ineligible. */
final class SteamOwnership {
 static final Set<ELicenseFlags> REJECTED=Collections.unmodifiableSet(EnumSet.of(ELicenseFlags.Pending,ELicenseFlags.Expired,ELicenseFlags.CancelledByAdmin,ELicenseFlags.CancelledByUser,ELicenseFlags.CancelledByFriendlyFraudLock,ELicenseFlags.CancelledByPartner,ELicenseFlags.PendingRefund,ELicenseFlags.NotActivated));
 static boolean accessible(int owner,long account,Collection<ELicenseFlags> flags){
  boolean shared=flags.contains(ELicenseFlags.Borrowed)||owner!=0&&Integer.toUnsignedLong(owner)!=account;
  return Collections.disjoint(REJECTED,flags)&&(shared||!flags.contains(ELicenseFlags.NonPermanent));
 }
 private SteamOwnership(){}
}
