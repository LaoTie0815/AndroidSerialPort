package com.sensetime.mtc;

import android.annotation.SuppressLint;
import android.text.TextUtils;
import android.util.Log;

import io.reactivex.Observable;
import io.reactivex.ObservableOnSubscribe;
import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.schedulers.Schedulers;


public class MClient {

    static {
        System.loadLibrary("serial");
    }

    private volatile static MClient mClient;

    public static MClient getInstance() {
        if (mClient == null) {
            synchronized (MClient.class) {
                if (mClient == null) {
                    mClient = new MClient();
                }
            }
        }
        return mClient;
    }

    private MClient() {
    }

    private static final String DEFAULT_DEV = "/dev/ttyACM0"; //如果确认系统中没有其他ACM设备,则可以用默认的.
    public static final int MTC_UPGRADE_SEGMENT_LENGTH = 16300;

    public static native void mSetIsDebug(boolean isDebug);

    protected native int mInit(String path, int speed, int stopBits, int dataBits, int parity);

    protected native int mPing();

    protected native void mDestroy();

    protected native boolean mRegisterCallback(IReceiveCallback callback);

    protected native String mHandleAt(String cmd);

    public boolean setReceiveCallback(IReceiveCallback callback) {
        return mRegisterCallback(callback);
    }


    /**
     * 指定设备路径初始化
     * specific device path to initialization.
     *
     * @param dev         指定的路径： specific device path
     * @param speed       传输速率： the uart transfer baudrate.
     * @param stopBits    传输停止位：the uart transfer data stop bit.
     * @param dataBits    数据位： the uart transfer data width bit.
     * @param parity      传输的数据是否需要奇偶校位：does the transmitted data need parity? 0 is none, 1 is odd，2 is even.
     * @param mtcCallback
     */
    @SuppressLint("CheckResult")
    public void mtcInit(final String dev, int speed, int stopBits, int dataBits, int parity, MtcCallback mtcCallback) {
        Observable.create((ObservableOnSubscribe<MtcReceiver>) emitter -> {

            String devPath = DEFAULT_DEV;
            if (!TextUtils.isEmpty(dev)) {
                devPath = dev;
            }
            Log.d(MClient.class.getSimpleName(), "devPath:" + devPath);
            int retCode = mInit(devPath, speed, stopBits, dataBits, parity);
            MtcCode mtcCode = MtcCode.matchRetCode(retCode);
            MtcReceiver mtcReceiver = new MtcReceiver();
            mtcReceiver.setMtcCode(mtcCode);
            emitter.onNext(mtcReceiver);
        }).subscribeOn(Schedulers.single()).observeOn(AndroidSchedulers.mainThread())
                .subscribe(s -> {
                    mtcCallback.onResult(s.getMtcCode(), s.getResult());
                });
    }


    /**
     * 检测设备的联通情况
     * check device's status, if device is working will return 0 .
     */
    @SuppressLint("CheckResult")
    public MtcCode mtcPing() {
        Object lock = new Object();
        MtcReceiver mtcReceiver = new MtcReceiver();
        // 默认设置成功，因为此时有可能正在做其他任务，导致进不了Schedulers.single()线程，而不是真正的ping不通
        mtcReceiver.setMtcCode(MtcCode.SUCCESS);
        Observable.create((ObservableOnSubscribe<MtcReceiver>) emitter -> {
            //真正进入了Schedulers.single()线程，才不默认code设置为超时，没有ping成功的话，告诉用户有可能断开连接了。
            mtcReceiver.setMtcCode(MtcCode.ERR_TIME_OUT);
            int retCode = mPing();
            MtcCode mtcCode = MtcCode.matchRetCode(retCode);
            mtcReceiver.setMtcCode(mtcCode);
            emitter.onNext(mtcReceiver);
        }).subscribeOn(Schedulers.single()).observeOn(AndroidSchedulers.mainThread())
                .subscribe(s -> {
                    synchronized (lock) {
                        lock.notify();
                    }
                    s.setResult("complete");
                });

        if (TextUtils.isEmpty(mtcReceiver.getResult())) {// it have not receive result, please wait a moment.
            try {
                synchronized (lock) {
                    lock.wait(2000);
                }
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        } else {
            Log.w(MClient.class.getSimpleName(), "received result is faster than wait");
        }
        return mtcReceiver.getMtcCode();
    }

    /**
     * 检测设备的联通情况
     * check device's status, if device is working will return 0 .
     */
    @SuppressLint("CheckResult")
    public void mtcPing(MtcCallback mtcCallback) {
        Observable.create((ObservableOnSubscribe<MtcReceiver>) emitter -> {
            int retCode = mPing();
            MtcReceiver mtcReceiver = new MtcReceiver();
            MtcCode mtcCode = MtcCode.matchRetCode(retCode);
            mtcReceiver.setMtcCode(mtcCode);
            emitter.onNext(mtcReceiver);
        }).subscribeOn(Schedulers.single()).observeOn(AndroidSchedulers.mainThread())
                .subscribe(s -> {
                    mtcCallback.onResult(s.getMtcCode(), s.getResult());
                });

    }

    /**
     * 如果要重新初始化,则应该销毁之前存在的实例.这是一个安全的指令,即便实例未初始化就销毁也不会异常.
     * The previous instance should be destroyed before reinitialization
     */
    public void mtcDestroy() {
        mDestroy();
    }

}
