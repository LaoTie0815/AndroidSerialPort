package com.sensetime.mtc;

/**
 * @Version V1.0.0
 * Author:Created by LaoTie on 2020/3/19.
 **/
public enum MtcCode {

    SUCCESS(0),
    ERR_NOT_INIT(-1),
    ERR_ALREADY_INIT(-2),
    ERR_CANNOT_OPEN(-3),
    ERR_CANNOT_FCNTL(-4),
    ERR_CANNOT_SET_ATTR(-5),
    ERR_INVALID_FD(-6),
    ERR_WRITE_RETRY_FAILED(-7),
    ERR_INVALID_DATA(-8),
    ERR_INVALID_LENGTH(-9),
    ERR_INVALID_ALLOC_MEM(-10),
    ERR_INVALID_PROTOCOL(-11),
    ERR_RECEIVER_INVALID_STAT(-12),
    ERR_TIME_OUT(-13),
    ERR_PROCESS_FAILED(-14),
    ERR_NO_IMAGE(-15),

    ERR_NO_IMAGE_ID(-1001),
    ERR_IMAGE_ID_LENGTH_TOO_LONG(-1002),
    ERR_IMAGE_IS_EMPTY(-1003),
    ERR_IMAGE_TOO_BIG(-1004),
    ERR_IMAGE_ID_IRREGULAR(-1005),

    AI_ERROR_CODE_1(1),
    AI_ERROR_CODE_2(2),
    AI_ERROR_CODE_3(3),
    AI_ERROR_CODE_4(4),
    AI_ERROR_CODE_5(5),
    AI_ERROR_CODE_6(6),
    AI_ERROR_CODE_7(7),
    AI_ERROR_CODE_8(8),
    AI_ERROR_CODE_9(9),
    AI_ERROR_CODE_10(10),
    AI_ERROR_CODE_11(11),
    AI_ERROR_CODE_12(12),
    AI_ERROR_CODE_13(13),
    AI_ERROR_CODE_14(14),
    AI_ERROR_CODE_15(15),
    AI_ERROR_CODE_16(16),
    AI_ERROR_CODE_17(17),
    AI_ERROR_CODE_18(18),
    AI_ERROR_CODE_19(19),
    AI_ERROR_CODE_20(20),


    ENGINE_ERR_CODE_160(160),


    PARAM_ERROR(-2001),

    UNKNOWN_ERROR(-9999);

    private int retCode;


    MtcCode(int retCode) {
        this.retCode = retCode;
    }

    public static MtcCode matchRetCode(int code) {
        for (MtcCode mtcCode : MtcCode.values()) {
            if (mtcCode.getRetCode() == code) {
                return mtcCode;
            }
        }
        return UNKNOWN_ERROR;
    }


    public int getRetCode() {
        return retCode;
    }

    public void setRetCode(int retCode) {
        this.retCode = retCode;
    }

}
