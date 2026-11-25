package org.mahoyo.port

import `in`.dragonbra.javasteam.enums.EResult
import `in`.dragonbra.javasteam.steam.authentication.AuthenticationException
import kotlinx.coroutines.*
import kotlinx.coroutines.future.await
import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.CompletableFuture

class SteamGuardFlowTest {
    @Test fun mobileApprovalRemainsDefaultWithoutOpeningCodeEntry() = runBlocking {
        val approval = CompletableDeferred<String>()
        val switch = CompletableFuture<Unit>()
        var codeOpened = false
        val result = async {
            SteamGuardFlow.approve({ approval.await() }, { switch.await(); codeOpened = true })
        }
        yield()
        approval.complete("approved")
        assertEquals("approved", result.await())
        assertFalse(codeOpened)
        assertTrue(switch.isCancelled)
    }

    @Test fun successfulCodeSubmissionStillWaitsForSteamApprovalResult() = runBlocking {
        val approval = CompletableDeferred<String>()
        val submitted = CompletableDeferred<Unit>()
        val result = async {
            SteamGuardFlow.approve({ approval.await() }, { submitted.complete(Unit); Unit })
        }
        submitted.await(); yield()
        assertFalse(result.isCompleted)
        approval.complete("token")
        assertEquals("token", result.await())
    }

    @Test fun mobileApprovalWhileCodeFormIsOpenCancelsCodeWait() = runBlocking {
        val approval = CompletableDeferred<String>()
        val opened = CompletableDeferred<Unit>()
        val answer = CompletableFuture<String>()
        val result = async {
            SteamGuardFlow.approve({ approval.await() }, { opened.complete(Unit); answer.await(); Unit })
        }
        opened.await()
        approval.complete("mobile")
        assertEquals("mobile", result.await())
        assertTrue(answer.isCancelled)
    }

    @Test fun cancellationStopsPollingAndPendingCodeInput() = runBlocking {
        val opened = CompletableDeferred<Unit>()
        val answer = CompletableFuture<String>()
        var stopped = false
        val result = async {
            SteamGuardFlow.approve({ try { awaitCancellation() } finally { stopped = true } }, {
                opened.complete(Unit); answer.await(); Unit
            })
        }
        opened.await(); result.cancelAndJoin()
        assertTrue(stopped)
        assertTrue(answer.isCancelled)
    }

    @Test fun incorrectCodesReuseTheInputLoop() = runBlocking {
        for (error in listOf(EResult.TwoFactorCodeMismatch, EResult.InvalidLoginAuthCode)) {
            val retry = mutableListOf<Boolean>()
            var sent = 0
            SteamGuardFlow.submitCode({ retry.add(it); "ABCDE" }, {
                if (sent++ == 0) throw AuthenticationException("authored mismatch", error)
            })
            assertEquals(listOf(false, true), retry)
            assertEquals(2, sent)
        }
    }

    @Test fun fatalCodeFailuresAreNotRetriedIndefinitely() = runBlocking {
        var attempts = 0
        val error = runCatching {
            SteamGuardFlow.submitCode({ "ABCDE" }, {
                attempts++; throw AuthenticationException("authored expiration", EResult.Expired)
            })
        }.exceptionOrNull()
        assertTrue(error is AuthenticationException)
        assertEquals(1, attempts)
    }
}
