package org.mahoyo.port

import `in`.dragonbra.javasteam.depotdownloader.DepotDownloader
import `in`.dragonbra.javasteam.steam.steamclient.SteamClient
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableSharedFlow
import org.junit.Assert.*
import org.junit.Test

class DownloadBackpressureTest {
    @Test fun compiledDownloaderBoundsBothQueuesWithoutDroppingItems()=runBlocking {
        val downloader=DepotDownloader(SteamClient(),emptyList(),autoStartDownload=false)
        try {
            for((name,capacity) in listOf("networkChunkFlow" to 32,"decompressFlow" to 8)) {
                val field=downloader.javaClass.getDeclaredField(name);field.isAccessible=true
                @Suppress("UNCHECKED_CAST") val queue=field.get(downloader) as MutableSharedFlow<Int>
                val release=Channel<Unit>();val received=ArrayList<Int>();var sent=0
                val consumer=launch(start=CoroutineStart.UNDISPATCHED){queue.collect{received.add(it);release.receive()}}
                val producer=launch{repeat(100){queue.emit(it);++sent}}
                try {
                    withTimeout(3000){while(sent<capacity+1)yield()}
                    assertEquals("producer suspends at the actual compiled queue bound",capacity+1,sent)
                    assertFalse(producer.isCompleted)
                    repeat(100){release.send(Unit)}
                    producer.join();assertEquals((0 until 100).toList(),received)
                }finally{producer.cancelAndJoin();consumer.cancelAndJoin();release.close()}
            }
        }finally{downloader.close()}
    }
}
