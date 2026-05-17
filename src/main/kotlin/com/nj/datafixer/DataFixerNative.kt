package com.nj.datafixer

object DataFixerNative {
    @JvmStatic external fun convertChunkPalette(
        blockStates: LongArray,
        palette: Array<String>,
        outputBlockIds: IntArray,
        blockCount: Int
    ): Boolean

    @JvmStatic external fun convertNibbleArray(
        input: ByteArray,
        output: ByteArray,
        count: Int
    ): Boolean

    @JvmStatic external fun convertBiomeArray(
        inputBiomes: IntArray,
        outputSections: IntArray,
        inputSize: Int,
        sectionCount: Int
    ): Boolean

    @JvmStatic external fun fixHeightmap(
        inputHeightmap: LongArray,
        outputHeightmap: LongArray,
        heightmapLength: Int
    ): Boolean

    @JvmStatic external fun batchReplaceStrings(
        inputStrings: Array<String>,
        fromKeys: Array<String>,
        toValues: Array<String>,
        count: Int
    ): Array<String>

    @JvmStatic external fun applyBitStorageFix(
        inputStorage: LongArray,
        inputBitsPerEntry: Int,
        outputBitsPerEntry: Int,
        outputStorage: LongArray,
        entryCount: Int
    ): Boolean

    @JvmStatic external fun convertPalettedChunkBlocks(
        rawBlockStates: LongArray,
        paletteSize: Int,
        paletteEntries: Array<String>,
        outBlockIds: IntArray,
        blockCount: Int
    ): Boolean

    @JvmField
    var isAvailable = false
}
