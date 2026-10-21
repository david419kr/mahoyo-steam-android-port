package org.mahoyo.port;
import in.dragonbra.javasteam.enums.ELicenseFlags;
import java.util.*;
/** Temporary access and another account's shared licenses cannot grant permanent offline activation. */
final class SteamOwnership {
 static final Set<ELicenseFlags> REJECTED=Collections.unmodifiableSet(EnumSet.of(ELicenseFlags.Pending,ELicenseFlags.Expired,ELicenseFlags.CancelledByAdmin,ELicenseFlags.CancelledByUser,ELicenseFlags.CancelledByFriendlyFraudLock,ELicenseFlags.CancelledByPartner,ELicenseFlags.PendingRefund,ELicenseFlags.Borrowed,ELicenseFlags.NonPermanent,ELicenseFlags.NotActivated));
 static boolean permanent(int owner,long account,Collection<ELicenseFlags> flags){return (owner==0||Integer.toUnsignedLong(owner)==account)&&Collections.disjoint(REJECTED,flags);}
 private SteamOwnership(){}
}
