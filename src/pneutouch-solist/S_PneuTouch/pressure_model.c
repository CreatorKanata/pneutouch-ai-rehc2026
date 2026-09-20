/* Only the reference training set is embedded. Every live prediction uses the
   current HX710B event, never a stored waveform, replay or a teacher label. */
#include "pressure_model.h"
#include "pressure_model_data.h"
#include "board.h"
#include "solistAi.h"
#include "smpl_common.h"
#include "irq.h"
#include <math.h>
#include <string.h>

volatile pneu_model_status_t pneutouch_model;
static bfloat16 input[12], teacher[4], output[4], reset_row[PNEU_MODEL_HIDDEN];

static bool wait_ai(void)
{
    uint32_t started = board_micros();
    while (ODL_IsBusy()) {
        board_watchdog();
        if (board_micros()-started >= 20000U) {
            pneutouch_model.ready = false; ++pneutouch_model.failures;
            return false;
        }
    }
    return true;
}

bool pneu_model_transform(const float values[12], uint16_t words[12])
{
    unsigned i;
    for (i = 0; i < 12; ++i) {
        uint32_t bits;
        float scaled;
        if (!(values[i] > 0.0f && values[i] < 100000.0f)) return false;
        scaled = (logf(values[i])-model_mean[i])/model_scale[i];
        memcpy(&bits, &scaled, sizeof(bits));
        if ((bits & 0x7f800000U) == 0x7f800000U) return false;
        words[i] = (uint16_t)(bits >> 16);
    }
    return true;
}

bool pneu_model_init(void)
{
    ODL_Parameters p = {0};
    uint32_t start = board_micros();
    unsigned i, j;
    pneutouch_model = (pneu_model_status_t){0};
    pneutouch_model.training_examples = PNEU_MODEL_EXAMPLES;
    pneutouch_model.training_steps = PNEU_MODEL_STEPS;
    p.inputSize = 12; p.hiddenSize = PNEU_MODEL_HIDDEN; p.outputSize = 4; p.seed = 1;
    p.forgettingFactor = p.scaleAlpha = p.leakRate = (bfloat16)0x3f80;
    p.activationFunction = ODL_ACTV_SIGMOID; p.lossFunction = ODL_LOSS_MSE;
    irq_ai_dis(); smpl_enablePeripheral(AI_PERI);
    ODL_Initialize(0, &p); ODL_Reset(0);
    memset(reset_row, 0, sizeof(reset_row));
    for (i = 0; i < PNEU_MODEL_HIDDEN; ++i) ODL_SetWeightBeta(reset_row, 0, i*8U, 8);
    for (i = 0; i < PNEU_MODEL_HIDDEN; ++i) {
        if (i) reset_row[i-1] = 0;
        reset_row[i] = (bfloat16)0x3f80;
        ODL_SetWeightP(reset_row, 0, i*PNEU_MODEL_HIDDEN*2U, PNEU_MODEL_HIDDEN*2U);
    }
    irq_ai_clearIRQ();
    for (i = 0; i < PNEU_MODEL_STEPS; ++i) {
        unsigned index = model_order[i];
        for (j = 0; j < 12; ++j) input[j] = (bfloat16)model_training[index][j];
        for (j = 0; j < 4; ++j) teacher[j] = j+1U == model_labels[index] ? (bfloat16)0x3f80 : 0;
        board_watchdog();
        ODL_StartTrain(0, input, teacher);
        if (!wait_ai()) return false;
    }
    pneutouch_model.training_us = board_micros()-start;
    pneutouch_model.ready = true;
    return true;
}

pneu_demo_class_t pneu_model_predict(const float values[12])
{
    uint16_t words[12];
    uint32_t start, bits;
    float best = 0, value;
    unsigned i, winner = 0;
    pneutouch_model.label = PNEU_CLASS_UNKNOWN;
    if (!pneutouch_model.ready || !pneu_model_transform(values, words)) return PNEU_CLASS_UNKNOWN;
    for (i = 0; i < 12; ++i) {
        input[i] = (bfloat16)words[i]; pneutouch_model.input[i] = words[i];
    }
    memset(teacher, 0, sizeof(teacher));
    start = board_micros();
    ODL_StartPredict(0, input, teacher);
    if (!wait_ai()) return PNEU_CLASS_UNKNOWN;
    pneutouch_model.inference_us = board_micros()-start;
    ODL_GetResult(0, output);
    for (i = 0; i < 4; ++i) {
        pneutouch_model.scores[i] = (uint16_t)output[i];
        bits = (uint32_t)(uint16_t)output[i] << 16;
        if ((bits & 0x7f800000U) == 0x7f800000U) {
            ++pneutouch_model.failures; return PNEU_CLASS_UNKNOWN;
        }
        memcpy(&value, &bits, sizeof(value));
        if (!i || value > best) { best = value; winner = i; }
    }
    ++pneutouch_model.predictions;
    pneutouch_model.label = (pneu_demo_class_t)(winner+1U);
    return pneutouch_model.label;
}

const char *pneu_class_name(pneu_demo_class_t label)
{
    static const char *const names[] = {"UNKNOWN", "TAIL", "BACK", "LEGS", "HEAD"};
    return label >= PNEU_CLASS_TAIL && label <= PNEU_CLASS_HEAD ? names[label] : names[0];
}
