package org.mahoyo.port

import `in`.dragonbra.javasteam.enums.EResult
import `in`.dragonbra.javasteam.steam.authentication.AuthenticationException
import kotlinx.coroutines.*
import kotlinx.coroutines.selects.select

/** Both confirmation methods belong to one Steam session; only polling produces its login result. */
internal object SteamGuardFlow {
    suspend fun <T> approve(poll: suspend () -> T, enterCode: suspend () -> Unit): T = coroutineScope {
        val approval = async { poll() }
        val code = async { enterCode() }
        try {
            select {
                approval.onAwait { it }
                code.onAwait { approval.await() }
            }
        } finally {
            code.cancel()
            approval.cancel()
        }
    }

    suspend fun submitCode(request: suspend (Boolean) -> String, send: suspend (String) -> Unit) {
        var incorrect = false
        while (true) {
            val code = request(incorrect)
            try { send(code); return }
            catch (error: AuthenticationException) {
                if (error.result != EResult.InvalidLoginAuthCode && error.result != EResult.TwoFactorCodeMismatch) throw error
                incorrect = true
            }
        }
    }
}
