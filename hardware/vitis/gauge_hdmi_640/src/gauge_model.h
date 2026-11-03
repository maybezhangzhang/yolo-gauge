#ifndef GAUGE_MODEL_H
#define GAUGE_MODEL_H
#define NUM_CLASSES 4
#define OUT_CH (3 * (5 + NUM_CLASSES))
#define BOARD_INPUT_SIZE 640
#define TRAINING_INPUT_SIZE 640
static const float ANCHORS[6][2] = {
    {19.600000000f, 12.100000000f},
    {28.800000000f, 15.900000000f},
    {24.000000000f, 24.600000000f},
    {35.500000000f, 19.100000000f},
    {46.300000000f, 26.300000000f},
    {50.500000000f, 34.700000000f}
};
static const int MASK0[3] = {3, 4, 5};
static const int MASK1[3] = {0, 1, 2};
#endif
