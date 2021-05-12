package com.laotie.serial.ui;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.laotie.serial.R;
import com.laotie.serial.LaoTieApp;

/**
 * @Version V1.0.0
 * Author:Created by LaoTie on 2020/1/2.
 **/
public class SplashActivity extends AppCompatActivity {

    private Handler mHandler = new Handler();
    private Runnable mRunnableToChoose = () -> toChooseActivity(null);

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_splash);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (LaoTieApp.FIRST_ACTIVITY_CREATED_NUM == 0) {
            mHandler.postDelayed(mRunnableToChoose, 2000);
            LaoTieApp.FIRST_ACTIVITY_CREATED_NUM ++;
        }
    }

    public void toChooseActivity(View v){
         Intent intent = new Intent(this, SerialActivity.class);
         startActivity(intent);
         finish();
    }


}
