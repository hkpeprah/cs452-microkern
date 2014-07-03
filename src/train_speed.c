#include <train_speed.h>
#include <types.h>
#include <term.h>

static struct train_speed_state trainSpeeds[6];


static void init_speed_state() {
    trainSpeeds[0].train = "45";
    trainSpeeds[0].ticks[0] = 0;
    trainSpeeds[0].speed[0] = 0;
    trainSpeeds[0].ticks[1] = 0;
    trainSpeeds[0].speed[1] = 0;
    trainSpeeds[0].ticks[2] = 0;
    trainSpeeds[0].speed[2] = 0;
    trainSpeeds[0].ticks[3] = 0;
    trainSpeeds[0].speed[3] = 0;
    trainSpeeds[0].ticks[4] = 0;
    trainSpeeds[0].speed[4] = 0;
    trainSpeeds[0].ticks[5] = 0;
    trainSpeeds[0].speed[5] = 0;
    trainSpeeds[0].ticks[6] = 0;
    trainSpeeds[0].speed[6] = 0;
    trainSpeeds[0].ticks[7] = 0;
    trainSpeeds[0].speed[7] = 0;
    trainSpeeds[0].ticks[8] = 0;
    trainSpeeds[0].speed[8] = 0;
    trainSpeeds[0].ticks[9] = 0;
    trainSpeeds[0].speed[9] = 0;
    trainSpeeds[0].ticks[10] = 0;
    trainSpeeds[0].speed[10] = 0;
    trainSpeeds[0].ticks[11] = 0;
    trainSpeeds[0].speed[11] = 0;
    trainSpeeds[0].ticks[12] = 0;
    trainSpeeds[0].speed[12] = 0;
    trainSpeeds[0].ticks[13] = 0;
    trainSpeeds[0].speed[13] = 0;
    trainSpeeds[0].ticks[14] = 0;
    trainSpeeds[0].speed[14] = 0;

    trainSpeeds[1].train = "47";
    trainSpeeds[1].ticks[0] = 0;
    trainSpeeds[1].speed[0] = 0;
    trainSpeeds[1].ticks[1] = 3578;
    trainSpeeds[1].speed[1] = 1303;
    trainSpeeds[1].ticks[2] = 3578;
    trainSpeeds[1].speed[2] = 1303;
    trainSpeeds[1].ticks[3] = 3578;
    trainSpeeds[1].speed[3] = 1303;
    trainSpeeds[1].ticks[4] = 2594;
    trainSpeeds[1].speed[4] = 1798;
    trainSpeeds[1].ticks[5] = 1965;
    trainSpeeds[1].speed[5] = 2373;
    trainSpeeds[1].ticks[6] = 1612;
    trainSpeeds[1].speed[6] = 2892;
    trainSpeeds[1].ticks[7] = 1350;
    trainSpeeds[1].speed[7] = 3541;
    trainSpeeds[1].ticks[8] = 1221;
    trainSpeeds[1].speed[8] = 3819;
    trainSpeeds[1].ticks[9] = 1075;
    trainSpeeds[1].speed[9] = 4338;
    trainSpeeds[1].ticks[10] = 990;
    trainSpeeds[1].speed[10] = 4710;
    trainSpeeds[1].ticks[11] = 910;
    trainSpeeds[1].speed[11] = 5124;
    trainSpeeds[1].ticks[12] = 904;
    trainSpeeds[1].speed[12] = 5158;
    trainSpeeds[1].ticks[13] = 892;
    trainSpeeds[1].speed[13] = 5228;
    trainSpeeds[1].ticks[14] = 885;
    trainSpeeds[1].speed[14] = 5268;

    trainSpeeds[2].train = "48";
    trainSpeeds[2].ticks[0] = 0;
    trainSpeeds[2].speed[0] = 0;
    trainSpeeds[2].ticks[1] = 0;
    trainSpeeds[2].speed[1] = 0;
    trainSpeeds[2].ticks[2] = 0;
    trainSpeeds[2].speed[2] = 0;
    trainSpeeds[2].ticks[3] = 0;
    trainSpeeds[2].speed[3] = 0;
    trainSpeeds[2].ticks[4] = 0;
    trainSpeeds[2].speed[4] = 0;
    trainSpeeds[2].ticks[5] = 1984;
    trainSpeeds[2].speed[5] = 2350;
    trainSpeeds[2].ticks[6] = 1618;
    trainSpeeds[2].speed[6] = 2882;
    trainSpeeds[2].ticks[7] = 1386;
    trainSpeeds[2].speed[7] = 3364;
    trainSpeeds[2].ticks[8] = 1233;
    trainSpeeds[2].speed[8] = 3782;
    trainSpeeds[2].ticks[9] = 1111;
    trainSpeeds[2].speed[9] = 4197;
    trainSpeeds[2].ticks[10] = 990;
    trainSpeeds[2].speed[10] = 4710;
    trainSpeeds[2].ticks[11] = 898;
    trainSpeeds[2].speed[11] = 5193;
    trainSpeeds[2].ticks[12] = 873;
    trainSpeeds[2].speed[12] = 5341;
    trainSpeeds[2].ticks[13] = 861;
    trainSpeeds[2].speed[13] = 5416;
    trainSpeeds[2].ticks[14] = 842;
    trainSpeeds[2].speed[14] = 5538;

    trainSpeeds[3].train = "49";
    trainSpeeds[3].ticks[0] = 0;
    trainSpeeds[3].speed[0] = 0;
    trainSpeeds[3].ticks[1] = 3657;
    trainSpeeds[3].speed[1] = 1275;
    trainSpeeds[3].ticks[2] = 3657;
    trainSpeeds[3].speed[2] = 1275;
    trainSpeeds[3].ticks[3] = 3657;
    trainSpeeds[3].speed[3] = 1275;
    trainSpeeds[3].ticks[4] = 2736;
    trainSpeeds[3].speed[4] = 1704;
    trainSpeeds[3].ticks[5] = 2009;
    trainSpeeds[3].speed[5] = 2321;
    trainSpeeds[3].ticks[6] = 1637;
    trainSpeeds[3].speed[6] = 2846;
    trainSpeeds[3].ticks[7] = 1361;
    trainSpeeds[3].speed[7] = 3426;
    trainSpeeds[3].ticks[8] = 1185;
    trainSpeeds[3].speed[8] = 3935;
    trainSpeeds[3].ticks[9] = 1032;
    trainSpeeds[3].speed[9] = 4518;
    trainSpeeds[3].ticks[10] = 934;
    trainSpeeds[3].speed[10] = 4993;
    trainSpeeds[3].ticks[11] = 855;
    trainSpeeds[3].speed[11] = 5454;
    trainSpeeds[3].ticks[12] = 848;
    trainSpeeds[3].speed[12] = 5498;
    trainSpeeds[3].ticks[13] = 842;
    trainSpeeds[3].speed[13] = 5538;
    trainSpeeds[3].ticks[14] = 818;
    trainSpeeds[3].speed[14] = 5700;

    trainSpeeds[4].train = "50";
    trainSpeeds[4].ticks[0] = 0;
    trainSpeeds[4].speed[0] = 0;
    trainSpeeds[4].ticks[1] = 0;
    trainSpeeds[4].speed[1] = 0;
    trainSpeeds[4].ticks[2] = 0;
    trainSpeeds[4].speed[2] = 0;
    trainSpeeds[4].ticks[3] = 0;
    trainSpeeds[4].speed[3] = 0;
    trainSpeeds[4].ticks[4] = 0;
    trainSpeeds[4].speed[4] = 0;
    trainSpeeds[4].ticks[5] = 0;
    trainSpeeds[4].speed[5] = 0;
    trainSpeeds[4].ticks[6] = 0;
    trainSpeeds[4].speed[6] = 0;
    trainSpeeds[4].ticks[7] = 0;
    trainSpeeds[4].speed[7] = 0;
    trainSpeeds[4].ticks[8] = 0;
    trainSpeeds[4].speed[8] = 0;
    trainSpeeds[4].ticks[9] = 0;
    trainSpeeds[4].speed[9] = 0;
    trainSpeeds[4].ticks[10] = 0;
    trainSpeeds[4].speed[10] = 0;
    trainSpeeds[4].ticks[11] = 0;
    trainSpeeds[4].speed[11] = 0;
    trainSpeeds[4].ticks[12] = 0;
    trainSpeeds[4].speed[12] = 0;
    trainSpeeds[4].ticks[13] = 0;
    trainSpeeds[4].speed[13] = 0;
    trainSpeeds[4].ticks[14] = 0;
    trainSpeeds[4].speed[14] = 0;

    trainSpeeds[5].train = "51";
    trainSpeeds[5].ticks[0] = 0;
    trainSpeeds[5].speed[0] = 0;
    trainSpeeds[5].ticks[1] = 10880;
    trainSpeeds[5].speed[1] = 429;
    trainSpeeds[5].ticks[2] = 10880;
    trainSpeeds[5].speed[2] = 429;
    trainSpeeds[5].ticks[3] = 10880;
    trainSpeeds[5].speed[3] = 429;
    trainSpeeds[5].ticks[4] = 5696;
    trainSpeeds[5].speed[4] = 819;
    trainSpeeds[5].ticks[5] = 4097;
    trainSpeeds[5].speed[5] = 1138;
    trainSpeeds[5].ticks[6] = 3047;
    trainSpeeds[5].speed[6] = 1530;
    trainSpeeds[5].ticks[7] = 2467;
    trainSpeeds[5].speed[7] = 1890;
    trainSpeeds[5].ticks[8] = 2082;
    trainSpeeds[5].speed[8] = 2240;
    trainSpeeds[5].ticks[9] = 1667;
    trainSpeeds[5].speed[9] = 2797;
    trainSpeeds[5].ticks[10] = 1398;
    trainSpeeds[5].speed[10] = 3335;
    trainSpeeds[5].ticks[11] = 1130;
    trainSpeeds[5].speed[11] = 4127;
    trainSpeeds[5].ticks[12] = 971;
    trainSpeeds[5].speed[12] = 4802;
    trainSpeeds[5].ticks[13] = 812;
    trainSpeeds[5].speed[13] = 5743;
    trainSpeeds[5].ticks[14] = 708;
    trainSpeeds[5].speed[14] = 6586;
}


unsigned int getTrainVelocity(unsigned int tr, unsigned int sp) {
    static bool started = false;
    unsigned int id;

    if (started == false) {
        started = true;
        init_speed_state();
    }

    if (sp > 14) {
        error("getTrainVelocity: Speed out of range.");
        return 0;
    }

    switch (tr) {
        case 45:
            id = 0;
            break;
        case 47:
            id = 1;
            break;
        case 48:
            id = 2;
            break;
        case 49:
            id = 3;
            break;
        case 50:
            id = 4;
            break;
        case 51:
            id = 5;
            break;
        default:
            error("getTrainVelocity: Unknown train.");
            return 0;
    }

    return trainSpeeds[id].speed[sp];
}
