package com.laotie.serial;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.util.Log;
import android.util.SparseArray;


import com.sensetime.mtc.MClient;
import com.squareup.leakcanary.LeakCanary;
import com.squareup.leakcanary.RefWatcher;

import java.lang.ref.WeakReference;
import java.util.Stack;

/**
 * Author:Created by LaoTie on 2019/12/12.
 * Enterprise:SenseTime
 **/
public class LaoTieApp extends Application {

    private MClient mClient = MClient.getInstance();


    public static int FIRST_ACTIVITY_CREATED_NUM = 0;
    private Stack<WeakReference<Activity>> activityList = new Stack<WeakReference<Activity>>();
    public static String MRROCONFIG_SP;

    private SparseArray<String> selectedImagePaths;
    private RefWatcher refWatcher;
    private boolean isExitApp = false;

    @Override
    public void onCreate() {
        super.onCreate();
        initLogger();
        FIRST_ACTIVITY_CREATED_NUM = 0;


        if (LeakCanary.isInAnalyzerProcess(this)) {
            // This process is dedicated to LeakCanary for heap analysis.
            // You should not init your app in this process.
            return;
        }
        refWatcher = LeakCanary.install(this);
        isExitApp = false;
    }



    /**
     * 根据不同的版本初始化日志模块
     */
    private void initLogger() {
        int logPriority = Log.VERBOSE;
        mClient.mSetIsDebug(true);
    }

    /**
     * 将Activity压入Application栈
     *
     * @param task 将要压入栈的Activity对象
     */
    public void pushTask(WeakReference<Activity> task) {
        int index = activityList.indexOf(task);
        activityList.push(task);
    }

    /**
     * 将传入的Activity对象从栈中移除
     *
     * @param task
     */
    public void removeTask(WeakReference<Activity> task) {
        activityList.remove(task);
    }

    @Override
    public void onTerminate() {
        super.onTerminate();
        MClient.getInstance().mtcDestroy();
    }

    public void exitApp() {
        for (WeakReference<Activity> task : activityList) {
            Activity activity = task.get();
            if (activity != null && !activity.isFinishing()) {
                activity.finish();
            }
        }
        isExitApp = true;
        System.exit(0);
    }

    public static RefWatcher getRefWatcher(Context context) {
        LaoTieApp laoTieApp = (LaoTieApp) context.getApplicationContext();
        return laoTieApp.refWatcher;
    }

    public SparseArray<String> getSelectedImagePaths() {
        return selectedImagePaths;
    }

    public void setSelectedImagePaths(SparseArray<String> selectedImagePaths) {
        this.selectedImagePaths = selectedImagePaths;
    }
}
