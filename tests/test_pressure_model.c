#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pressure_model.h"
#include "pressure_model_data.h"
#include "solistAi.h"
static unsigned trained[2], predicted[2], now, winner[2];
static int stalled_model = -1, nonfinite_model = -1;
static unsigned active_model;
static uint16_t expected_primary[12];
static bool check_primary;
static unsigned hidden_for(unsigned model) { return model ? PNEU_SPECIALIST_HIDDEN : PNEU_MODEL_HIDDEN; }
uint32_t board_micros(void) { return now++; }
void board_watchdog(void) {}
void smpl_enablePeripheral(unsigned mask) { assert(mask == 0x40000000U); }
void ODL_Initialize(uint8_t model, const ODL_Parameters *p)
{
    assert(model < 2 && p->inputSize == 12 && p->hiddenSize == hidden_for(model) && p->outputSize == 4);
    assert(p->seed == 1 && p->activationFunction == 1 && p->scaleGamma == 0);
    trained[model] = 0;
}
void ODL_Reset(uint8_t model) { assert(model < 2); }
void ODL_SetWeightBeta(const void *v, uint8_t model, uint32_t off, uint16_t size)
{
    assert(model < 2 && off < hidden_for(model)*8U && size == 8);
    for (unsigned i = 0; i < 4; ++i) assert(((const bfloat16 *)v)[i] == 0);
}
void ODL_SetWeightP(const void *v, uint8_t model, uint32_t off, uint16_t size)
{
    assert(model < 2 && size == hidden_for(model)*2U);
    for (unsigned i = 0; i < hidden_for(model); ++i)
        assert(((const bfloat16 *)v)[i] == (i == off/size ? 0x3f80 : 0));
}
void ODL_StartTrain(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    assert(model < 2);
    const uint8_t *order = model ? specialist_order : model_order;
    const uint8_t *labels = model ? specialist_labels : model_labels;
    const uint16_t (*data)[12] = model ? specialist_training : model_training;
    unsigned index = order[trained[model]++];
    assert(!memcmp(x, data[index], 24));
    if (model) assert(labels[index] == 3 || labels[index] == 4);
    for (unsigned i = 0; i < 4; ++i) assert(t[i] == (i+1 == labels[index] ? 0x3f80 : 0));
    active_model = model;
}
void ODL_StartPredict(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    assert(model < 2 && x != NULL);
    for (unsigned i = 0; i < 4; ++i) assert(t[i] == 0);
    if (model) assert(!memcmp(x, specialist_training[0], 24));
    else if (check_primary) assert(!memcmp(x, expected_primary, 24));
    ++predicted[model]; active_model = model;
}
uint32_t ODL_IsBusy(void) { return (int)active_model == stalled_model; }
void ODL_GetResult(uint8_t model, bfloat16 *y)
{
    assert(model == active_model);
    for (unsigned i = 0; i < 4; ++i) y[i] = (bfloat16)(i == winner[model] ? 0xbf00 : 0xbf80);
    if (model) y[0] = 0x3f80; /* Specialist must ignore unused TAIL/BACK outputs. */
    if ((int)model == nonfinite_model) y[3] = 0x7fc0;
}
int main(void)
{
    float x[12]; uint16_t words[12];
    memcpy(x, model_reference_raw, sizeof(x));
    assert(pneu_model_transform(x, words));
    assert(!memcmp(words, model_training[0], 24));
    assert(pneu_model_init() && pneutouch_model.ready);
    assert(trained[0] == PNEU_MODEL_STEPS && trained[1] == PNEU_SPECIALIST_STEPS);
    assert(pneutouch_model.training_steps == PNEU_MODEL_STEPS + PNEU_SPECIALIST_STEPS);
    /* The reference shape already carries peak/1000 and trough/1000 at inputs
       5/10, so the primary must see it normalized with model_mean/scale and
       the specialist must reproduce specialist_training[0] bit-exactly. */
    memcpy(x, specialist_reference_shape, sizeof(x));
    assert(pneu_model_transform(x, expected_primary));
    check_primary = true;
    x[5] = x[10] = 1.0f; /* Overwritten by the absolute peak/trough arguments. */
    for (winner[0] = 0; winner[0] < 2; ++winner[0]) {
        assert(pneu_model_predict(x, specialist_reference_peak, specialist_reference_trough) ==
               (pneu_demo_class_t)(winner[0]+1));
        assert(!pneutouch_model.specialist_used && predicted[1] == 0);
    }
    for (winner[0] = 2; winner[0] < 4; ++winner[0])
        for (winner[1] = 2; winner[1] < 4; ++winner[1]) {
            assert(pneu_model_predict(x, specialist_reference_peak, specialist_reference_trough) ==
                   (pneu_demo_class_t)(winner[1]+1));
            assert(pneutouch_model.specialist_used);
            assert(pneutouch_model.primary_label == (pneu_demo_class_t)(winner[0]+1));
        }
    assert(predicted[0] == 6 && predicted[1] == 4);
    winner[0] = 2; winner[1] = 3;
    /* Invalid amplitudes are rejected before either instance runs. */
    assert(pneu_model_predict(x, NAN, specialist_reference_trough) == PNEU_CLASS_UNKNOWN);
    assert(pneu_model_predict(x, specialist_reference_peak, NAN) == PNEU_CLASS_UNKNOWN);
    assert(pneu_model_predict(x, 0.0f, specialist_reference_trough) == PNEU_CLASS_UNKNOWN);
    assert(pneu_model_predict(x, specialist_reference_peak, 1e9f) == PNEU_CLASS_UNKNOWN);
    assert(predicted[0] == 6 && predicted[1] == 4);
    assert(pneutouch_model.label == PNEU_CLASS_UNKNOWN && pneutouch_model.primary_label == PNEU_CLASS_UNKNOWN);
    x[0] = NAN;
    assert(pneu_model_predict(x, specialist_reference_peak, specialist_reference_trough) == PNEU_CLASS_UNKNOWN);
    assert(predicted[0] == 6);
    x[0] = 0; assert(!pneu_model_transform(x, words));
    x[0] = INFINITY; assert(!pneu_model_transform(x, words));
    memcpy(x, specialist_reference_shape, sizeof(x));
    nonfinite_model = 1;
    assert(pneu_model_predict(x, specialist_reference_peak, specialist_reference_trough) == PNEU_CLASS_UNKNOWN);
    nonfinite_model = -1; stalled_model = 1;
    assert(pneu_model_predict(x, specialist_reference_peak, specialist_reference_trough) == PNEU_CLASS_UNKNOWN && !pneutouch_model.ready);
    assert(!pneu_model_init()); /* Failure of the second boot model is visible. */
    stalled_model = -1; assert(pneu_model_init());
    nonfinite_model = 0;
    assert(pneu_model_predict(x, specialist_reference_peak, specialist_reference_trough) == PNEU_CLASS_UNKNOWN);
    nonfinite_model = -1; stalled_model = 0;
    assert(pneu_model_predict(x, specialist_reference_peak, specialist_reference_trough) == PNEU_CLASS_UNKNOWN && !pneutouch_model.ready);
    assert(!strcmp(pneu_class_name(PNEU_CLASS_HEAD), "HEAD"));
    puts("Pressure model: two-instance training, exact specialist inputs, gated HEAD/LEGS correction, no teacher leakage, faults PASS");
}
