package com.laotie.serial.ui;

import android.annotation.SuppressLint;
import android.content.res.Configuration;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.laotie.serial.R;
import com.laotie.serial.LaoTieApp;
import com.sensetime.mtc.MClient;
import com.sensetime.mtc.MtcCode;

import java.io.File;

/**
 * An example full-screen activity that shows and hides the system UI (i.e.
 * status bar and navigation/system bar) with user interaction.
 */
@SuppressLint("Registered")
public class SerialActivity extends AppCompatActivity {

    private MClient mClient = MClient.getInstance();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_serial);
    }


    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    protected void onRestart() {
        super.onRestart();
    }

    @Override
    protected void onStop() {
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
                LaoTieApp laoTieApp = (LaoTieApp) getApplication();
                laoTieApp.exitApp();
                return true;
            default:
                break;
        }
        return super.onKeyDown(keyCode, event);
    }


    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    public String getDevicePath() {
        int devNameIndex = 3;
        String dev = "";
        for (int i = 0; i <= devNameIndex; i++) {
            dev = "/dev/ttyACM" + i;
            File device = new File(dev);
            if (device.exists()) {
                if (!device.canRead() || !device.canWrite() || !device.canExecute()) {
                    Log.e(SerialActivity.class.getSimpleName(), "can't read/write device: " + dev + ", start get root permission.");
                } else {
                    Log.v(SerialActivity.class.getSimpleName(), dev + " is exists");
                    return dev;
                }
            } else {
                Log.v(SerialActivity.class.getSimpleName(), dev + " is't exists");
            }
        }
        return "";
    }

    @SuppressLint("CheckResult")
    public void initSerial(View view) {
        String dev = getDevicePath();
        if (!TextUtils.isEmpty(dev)) {
            int speed = 115200;
            int stopBits = 1;
            int dataBits = 8;
            int parity = 0;
            mClient.mtcInit(dev, speed, stopBits, dataBits, parity, (mtcCode, result) -> {
                if (mtcCode == MtcCode.SUCCESS) {
                    Toast.makeText(this, "init serial success", Toast.LENGTH_LONG).show();
                } else {
                    Toast.makeText(this, "init serial failure", Toast.LENGTH_LONG).show();
                }
            });
        }
    }


    public void checkPing(View v) {
        mClient.mtcPing((mtcCode, result) -> {
            if (mtcCode == MtcCode.SUCCESS) {
                Toast.makeText(this, "success", Toast.LENGTH_LONG).show();
            } else {
                Toast.makeText(this, "failure", Toast.LENGTH_LONG).show();
            }
        });

    }

    @Override
    protected void onPostCreate(Bundle savedInstanceState) {
        super.onPostCreate(savedInstanceState);
    }

}
