package com.alphastudio.snapheateru1.data

import java.io.Reader

internal fun readBoundedResponse(reader: Reader, limit: Int): String {
    require(limit >= 0)
    val result = StringBuilder()
    val buffer = CharArray(1024)
    while (true) {
        val count = reader.read(buffer, 0, minOf(buffer.size, limit - result.length + 1))
        if (count < 0) return result.toString()
        require(result.length + count <= limit) { "Response exceeds limit" }
        result.append(buffer, 0, count)
    }
}
