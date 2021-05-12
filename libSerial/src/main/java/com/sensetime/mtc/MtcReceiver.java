package com.sensetime.mtc;

/**
 * @Version V1.0.0
 * Author:Created by LaoTie on 2020/3/19.
 **/
public class MtcReceiver {
    private MtcCode mtcCode = MtcCode.ERR_TIME_OUT;
    private String result;

    public MtcCode getMtcCode() {
        return mtcCode;
    }

    public void setMtcCode(MtcCode mtcCode) {
        this.mtcCode = mtcCode;
    }

    public String getResult() {
        return result;
    }

    public void setResult(String result) {
        this.result = result;
    }

}
