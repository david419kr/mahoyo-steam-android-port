package org.mahoyo.port

import android.os.Handler
import android.os.Looper
import `in`.dragonbra.javasteam.depotdownloader.DepotDownloader
import `in`.dragonbra.javasteam.depotdownloader.IDownloadListener
import `in`.dragonbra.javasteam.depotdownloader.data.AppItem
import `in`.dragonbra.javasteam.depotdownloader.data.DownloadItem
import `in`.dragonbra.javasteam.enums.EResult
import `in`.dragonbra.javasteam.enums.EOSType
import `in`.dragonbra.javasteam.networking.steam3.ProtocolTypes
import `in`.dragonbra.javasteam.steam.authentication.AuthPollResult
import `in`.dragonbra.javasteam.steam.authentication.IChallengeUrlChanged
import `in`.dragonbra.javasteam.steam.authentication.AuthSessionDetails
import `in`.dragonbra.javasteam.steam.authentication.AuthSession
import `in`.dragonbra.javasteam.steam.authentication.CredentialsAuthSession
import `in`.dragonbra.javasteam.steam.authentication.AuthenticationException
import `in`.dragonbra.javasteam.protobufs.steamclient.SteammessagesAuthSteamclient.EAuthSessionGuardType
import `in`.dragonbra.javasteam.steam.handlers.steamapps.License
import `in`.dragonbra.javasteam.steam.handlers.steamapps.PICSRequest
import `in`.dragonbra.javasteam.steam.handlers.steamapps.SteamApps
import `in`.dragonbra.javasteam.steam.handlers.steamcontent.SteamContent
import `in`.dragonbra.javasteam.steam.handlers.steamapps.callback.LicenseListCallback
import `in`.dragonbra.javasteam.steam.handlers.steamuser.LogOnDetails
import `in`.dragonbra.javasteam.steam.handlers.steamuser.SteamUser
import `in`.dragonbra.javasteam.steam.handlers.steamuser.callback.LoggedOnCallback
import `in`.dragonbra.javasteam.steam.steamclient.SteamClient
import `in`.dragonbra.javasteam.steam.steamclient.callbackmgr.CallbackManager
import `in`.dragonbra.javasteam.steam.steamclient.callbacks.ConnectedCallback
import `in`.dragonbra.javasteam.steam.steamclient.callbacks.DisconnectedCallback
import `in`.dragonbra.javasteam.steam.steamclient.configuration.SteamConfiguration
import kotlinx.coroutines.*
import kotlinx.coroutines.future.await
import org.bouncycastle.jce.provider.BouncyCastleProvider
import java.io.Closeable
import java.io.File
import java.security.SecureRandom
import java.security.Security
import java.util.concurrent.CompletableFuture
import java.util.concurrent.atomic.AtomicLong

/** Created only by an explicit installation action. No session token is persisted. */
class SteamSession(private val listener: Listener,private val supportedManifest:Long,private val supportedInstallBytes:Long): Closeable {
    interface Listener {
        fun status(stage: String)
        fun guard(email: String?, incorrect: Boolean, answer: CompletableFuture<String>)
        fun qr(challenge: String)
        fun mobileApproval()
        fun owned(steamId: String)
        fun estimate(downloadBytes: Long, installedBytes: Long)
        fun progress(fraction: Float, compressedBytes: Long, installedBytes: Long)
        fun downloaded()
        fun failed(message: String)
    }
    private val scope=CoroutineScope(SupervisorJob()+Dispatchers.IO)
    private val main=Handler(Looper.getMainLooper())
    @Volatile private var client: SteamClient?=null
    @Volatile private var downloader: DepotDownloader?=null
    @Volatile private var verified=false
    private var licenses: List<License> = emptyList()
    private val connected=CompletableDeferred<Unit>()
    private val loggedOn=CompletableDeferred<LoggedOnCallback>()
    private val licenseList=CompletableDeferred<LicenseListCallback>()
    private var guard: CompletableFuture<String>?=null
    private fun post(action:()->Unit){main.post{if(scope.isActive)action()}}
    private fun report(error:Exception){if(scope.isActive)post{listener.failed(if(error is CancellationException)AuthQrRetry.INTERRUPTED else error.message?:error.javaClass.simpleName)}}
    fun login(username: String,password: CharArray){scope.launch{
        try {
            val steam=connectToSteam()
            val details=AuthSessionDetails().apply {
                this.username=username.trim();this.password=String(password);deviceFriendlyName="Mahoyo Unofficial Android Port"
                clientOSType=EOSType.AndroidUnknown;persistentSession=false
            }
            password.fill('\u0000')
            post{listener.status("authenticating")}
            val auth=try{withTimeout(300000){val session=steam.authentication.beginAuthSessionViaCredentials(details,scope).await();details.password=null;credentials(session)}}finally{details.password=null}
            verifyOwnership(steam,auth)
        }catch(error:Exception){password.fill('\u0000');report(error)}
    }}
    private suspend fun connectToSteam():SteamClient {
        Security.removeProvider("BC");Security.addProvider(BouncyCastleProvider())
        val steam=SteamClient(SteamConfiguration.create{it.withProtocolTypes(ProtocolTypes.WEB_SOCKET);it.withConnectionFactory{_,_->AndroidSteamConnection()}})
        client=steam
        val callbacks=CallbackManager(steam)
        callbacks.subscribe(ConnectedCallback::class.java){connected.complete(Unit)}
        callbacks.subscribe(LoggedOnCallback::class.java){loggedOn.complete(it)}
        callbacks.subscribe(LicenseListCallback::class.java){licenseList.complete(it)}
        callbacks.subscribe(DisconnectedCallback::class.java){
            val error=IllegalStateException(AuthQrRetry.DISCONNECTED)
            connected.completeExceptionally(error);loggedOn.completeExceptionally(error);licenseList.completeExceptionally(error)
            post{listener.failed(AuthQrRetry.DISCONNECTED)}
        }
        scope.launch{while(isActive)callbacks.runWaitCallbacks(200)}
        post{listener.status("connecting")};steam.connect();withTimeout(60000){connected.await()}
        return steam
    }
    private suspend fun verifyOwnership(steam:SteamClient,auth:AuthPollResult){
        val user=requireNotNull(steam.getHandler(SteamUser::class.java))
        user.logOn(LogOnDetails(username=auth.accountName,accessToken=auth.refreshToken,loginID=SecureRandom().nextInt(),shouldRememberPassword=false,clientOSType=EOSType.AndroidUnknown,machineName="Mahoyo Unofficial Android Port"))
        val logged=withTimeout(60000){loggedOn.await()};check(logged.result==EResult.OK){"Steam logon: "+logged.result}
        post{listener.status("ownership")}
        val list=withTimeout(60000){licenseList.await()};check(list.result==EResult.OK){"Steam licenses: "+list.result}
        val account=requireNotNull(steam.steamID).accountID
        val owned=list.licenseList.filter{SteamOwnership.permanent(it.ownerAccountID,account,it.licenseFlags)}
        check(owned.isNotEmpty()){ "No permanent game license found" }
        val apps=requireNotNull(steam.getHandler(SteamApps::class.java))
        var gameOwned=false
        for(group in owned.chunked(100)){
            val info=withTimeout(60000){apps.picsGetProductInfo(emptyList(),group.map{PICSRequest(it.packageID,it.accessToken)}).await()}
            if(info.results.any{result->result.packages.values.any{pkg->pkg.keyValues["appids"].children.any{it.asInteger()==Installation.APP_ID}}}){gameOwned=true;break}
        }
        check(gameOwned){"This Steam account does not own WITCH ON THE HOLY NIGHT"}
        val ticket=withTimeout(60000){apps.getAppOwnershipTicket(Installation.APP_ID).await()}
        check(ticket.result==EResult.OK&&ticket.appID==Installation.APP_ID&&ticket.ticket.isNotEmpty()){"Steam ownership confirmation failed: "+ticket.result}
        licenses=owned;verified=true
        post{listener.owned(java.lang.Long.toUnsignedString(requireNotNull(steam.steamID).convertToUInt64()))}
        querySize(steam,apps)
    }
    fun loginQr(){scope.launch {
        try{
            val steam=connectToSteam()
            val details=AuthSessionDetails().apply{deviceFriendlyName="Mahoyo Unofficial Android Port";clientOSType=EOSType.AndroidUnknown;persistentSession=false}
            val auth=withTimeout(300000){
                val qr=steam.authentication.beginAuthSessionViaQR(details,scope).await()
                qr.challengeUrlChanged=IChallengeUrlChanged{updated->if(updated!=null)post{listener.qr(updated.challengeUrl)}}
                post{listener.qr(qr.challengeUrl)}
                approval(qr,true)
            }
            post{listener.status("authenticating")}
            verifyOwnership(steam,auth)
        }catch(error:Exception){report(error)}
    }}
    private suspend fun approval(session:AuthSession,waitFirst:Boolean):AuthPollResult{
        if(waitFirst)delay(AuthTiming.pollMillis(session.pollingInterval))
        while(true){session.pollAuthSessionStatus(scope).await()?.let{return it};delay(AuthTiming.pollMillis(session.pollingInterval))}
    }
    private suspend fun credentials(session:CredentialsAuthSession):AuthPollResult{
        val none=EAuthSessionGuardType.k_EAuthSessionGuardType_None
        val mobile=EAuthSessionGuardType.k_EAuthSessionGuardType_DeviceConfirmation
        val device=EAuthSessionGuardType.k_EAuthSessionGuardType_DeviceCode
        val email=EAuthSessionGuardType.k_EAuthSessionGuardType_EmailCode
        val method=listOf(none,mobile,device,email).firstNotNullOfOrNull{type->session.allowedConfirmations.firstOrNull{it.confirmationType==type}}?:error("No supported Steam confirmation method")
        if(method.confirmationType==mobile){post{listener.mobileApproval()};return approval(session,true)}
        if(method.confirmationType!=none){
            var incorrect=false
            while(true){
                val code=requestGuard(if(method.confirmationType==email)method.associatedMessage else null,incorrect).await()
                try{session.sendSteamGuardCode(code,method.confirmationType,scope).await();break}
                catch(error:AuthenticationException){if(error.result!=EResult.InvalidLoginAuthCode&&error.result!=EResult.TwoFactorCodeMismatch)throw error;incorrect=true}
            }
        }
        return approval(session,false)
    }
    private fun requestGuard(email:String?,incorrect:Boolean):CompletableFuture<String>{val answer=CompletableFuture<String>();guard=answer;post{listener.guard(email,incorrect,answer)};return answer}
    private data class DepotPlan(val manifest:Long,val network:Long,val installed:Long)
    private suspend fun depotPlan(apps:SteamApps):DepotPlan=withTimeout(60000){
            val tokens=apps.picsGetAccessTokens(listOf(Installation.APP_ID),emptyList()).await()
            val info=apps.picsGetProductInfo(listOf(PICSRequest(Installation.APP_ID,tokens.appTokens[Installation.APP_ID]?:0)),emptyList()).await()
            val app=info.results.firstNotNullOfOrNull{it.apps[Installation.APP_ID]}?:error("Steam game information is unavailable")
            val depot=app.keyValues["depots"]["2052411"]["manifests"]["public"]
            DepotPlan(depot["gid"].asUnsignedLong().toLong(),depot["download"].asLong(),depot["size"].asLong())
    }
    private suspend fun querySize(steam:SteamClient,apps:SteamApps){
        try{val plan=depotPlan(apps);if(plan.manifest==supportedManifest)post{listener.estimate(plan.network,plan.installed)}
        }catch(ignored:Exception){/* The installer can still use the validated profile's size estimate. */}
    }
    fun connected():Boolean=verified&&client?.isConnected==true
    fun download(destination:File){scope.launch{
        try{
            check(connected()){ "Sign in to Steam again to download" }
            val content=requireNotNull(client?.getHandler(SteamContent::class.java))
            val requestCode=withTimeout(60000){content.getManifestRequestCode(2052411,Installation.APP_ID,supportedManifest,branch="public",parentScope=scope).await()}
            check(requestCode!=0L){"PINNED_MANIFEST_UNAVAILABLE"}
            // Upstream's completion future is not completed on every failure path.
            val finished=CompletableDeferred<Unit>();val lastActivity=AtomicLong(android.os.SystemClock.elapsedRealtime())
            val transfer=DepotDownloader(requireNotNull(client),licenses,debug=false,useLanCache=false,maxDownloads=4,maxDecompress=2,androidEmulation=true,parentJob=scope.coroutineContext[Job],autoStartDownload=false,skipLargeFileAllocation=true)
            downloader=transfer
            transfer.addListener(object:IDownloadListener{
                override fun onStatusUpdate(message:String){lastActivity.set(android.os.SystemClock.elapsedRealtime())}
                override fun onDownloadFailed(item:DownloadItem,error:Throwable){finished.completeExceptionally(error)}
                override fun onDownloadCompleted(item:DownloadItem){finished.complete(Unit)}
                override fun onChunkCompleted(depotId:Int,depotPercentComplete:Float,compressedBytes:Long,uncompressedBytes:Long){lastActivity.set(android.os.SystemClock.elapsedRealtime());post{listener.progress(TransferStats.overall(depotPercentComplete,compressedBytes,uncompressedBytes,supportedInstallBytes),compressedBytes,uncompressedBytes)}}
                override fun onFileCompleted(depotId:Int,fileName:String,depotPercentComplete:Float){lastActivity.set(android.os.SystemClock.elapsedRealtime())} // SDK reports each finished file as 100%, not the depot.
            })
            transfer.add(AppItem(Installation.APP_ID,installDirectory=destination.absolutePath,branch="public",os="windows",osArch="64",downloadAllLanguages=true,depot=listOf(2052411),manifest=listOf(supportedManifest),verify=true))
            transfer.finishAdding();transfer.startDownloading()
            while(withTimeoutOrNull(15000){finished.await();true}!=true){check(android.os.SystemClock.elapsedRealtime()-lastActivity.get()<300000){"Download stopped responding. Sign in again to resume the saved files."}}
            transfer.close();downloader=null
            post{listener.downloaded()}
        }catch(error:Exception){downloader?.close();downloader=null;report(error)}
    }}
    override fun close(){verified=false;guard?.cancel(true);guard=null;downloader?.close();downloader=null;client?.disconnect();client=null;scope.cancel()}
}
