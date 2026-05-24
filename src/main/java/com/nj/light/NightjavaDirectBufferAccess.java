package com.nj.light;

import java.nio.ByteBuffer;

public interface NightjavaDirectBufferAccess {
    ByteBuffer nightjava$getDirectBuffer();
    void nightjava$syncFromDirect();
}