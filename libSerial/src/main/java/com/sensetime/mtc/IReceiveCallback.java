package com.sensetime.mtc;

import java.nio.ByteBuffer;

public interface IReceiveCallback {
    void onTracking(String data);
    void onVerify(String data);
    void onQrCodeData(String data);
    void onPhotoData(int trackId, String recognizeId, ByteBuffer rgbHeadpose, ByteBuffer rgbBackground, ByteBuffer irBackgroud);
    void onFeatureData(String traceId, ByteBuffer feature);
    void onDisconnect(ByteBuffer notUseNow);
}
