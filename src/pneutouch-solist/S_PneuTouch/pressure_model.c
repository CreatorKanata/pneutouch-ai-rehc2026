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
static bfloat16 input[12], teacher[4], output[4], reset_row[64];

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

static bool normalize(const float logged[12], uint16_t words[12],
                      const float mean[12], const float scale[12])
{
    unsigned i;
    for (i = 0; i < 12; ++i) {
        uint32_t bits;
        float scaled;
        scaled = (logged[i]-mean[i])/scale[i];
        memcpy(&bits, &scaled, sizeof(bits));
        if ((bits & 0x7f800000U) == 0x7f800000U) return false;
        words[i] = (uint16_t)(bits >> 16);
    }
    return true;
}

static bool logarithms(const float values[12], float logged[12])
{
    unsigned i;
    for (i = 0; i < 12; ++i) {
        if (!(values[i] > 0.0f && values[i] < 100000.0f)) return false;
        logged[i] = logf(values[i]);
    }
    return true;
}

bool pneu_model_transform(const float values[12], uint16_t words[12])
{
    float logged[12];
    return logarithms(values, logged) && normalize(logged, words, model_mean, model_scale);
}

static bool train_instance(uint8_t instance, unsigned hidden, unsigned steps,
                           const uint16_t data[][12], const uint8_t labels[],
                           const uint8_t order[])
{
    ODL_Parameters p = {0};
    unsigned i, j;
    p.inputSize = 12; p.hiddenSize = (uint16_t)hidden; p.outputSize = 4; p.seed = 1;
    p.forgettingFactor = p.scaleAlpha = p.leakRate = (bfloat16)0x3f80;
    p.activationFunction = ODL_ACTV_SIGMOID; p.lossFunction = ODL_LOSS_MSE;
    ODL_Initialize(instance, &p); ODL_Reset(instance);
    memset(reset_row, 0, sizeof(reset_row));
    for (i = 0; i < hidden; ++i) ODL_SetWeightBeta(reset_row, instance, i*8U, 8);
    for (i = 0; i < hidden; ++i) {
        if (i) reset_row[i-1] = 0;
        reset_row[i] = (bfloat16)0x3f80;
        ODL_SetWeightP(reset_row, instance, i*hidden*2U, (uint16_t)(hidden*2U));
    }
    irq_ai_clearIRQ();
    for (i = 0; i < steps; ++i) {
        unsigned index = order[i];
        for (j = 0; j < 12; ++j) input[j] = (bfloat16)data[index][j];
        for (j = 0; j < 4; ++j) teacher[j] = j+1U == labels[index] ? (bfloat16)0x3f80 : 0;
        board_watchdog();
        ODL_StartTrain(instance, input, teacher);
        if (!wait_ai()) return false;
    }
    return true;
}

bool pneu_model_init(void)
{
    uint32_t start = board_micros();
    pneutouch_model = (pneu_model_status_t){0};
    pneutouch_model.training_examples = PNEU_MODEL_EXAMPLES;
    pneutouch_model.training_steps = PNEU_MODEL_STEPS + PNEU_SPECIALIST_STEPS;
    pneutouch_model.specialist_examples = PNEU_SPECIALIST_EXAMPLES;
    pneutouch_model.specialist_steps = PNEU_SPECIALIST_STEPS;
    irq_ai_dis(); smpl_enablePeripheral(AI_PERI);
    if (!train_instance(0, PNEU_MODEL_HIDDEN, PNEU_MODEL_STEPS,
                        model_training, model_labels, model_order) ||
        !train_instance(1, PNEU_SPECIALIST_HIDDEN, PNEU_SPECIALIST_STEPS,
                        specialist_training, specialist_labels, specialist_order)) return false;
    pneutouch_model.training_us = board_micros()-start;
    pneutouch_model.ready = true;
    return true;
}

static pneu_demo_class_t predict_instance(uint8_t instance, const uint16_t words[12], unsigned first)
{
    uint32_t bits;
    float best = 0, value;
    unsigned i, winner = 0;
    for (i = 0; i < 12; ++i) {
        input[i] = (bfloat16)words[i]; pneutouch_model.input[i] = words[i];
    }
    memset(teacher, 0, sizeof(teacher));
    ODL_StartPredict(instance, input, teacher);
    if (!wait_ai()) return PNEU_CLASS_UNKNOWN;
    ODL_GetResult(instance, output);
    for (i = 0; i < 4; ++i) {
        pneutouch_model.scores[i] = (uint16_t)output[i];
        bits = (uint32_t)(uint16_t)output[i] << 16;
        if ((bits & 0x7f800000U) == 0x7f800000U) {
            ++pneutouch_model.failures; return PNEU_CLASS_UNKNOWN;
        }
        memcpy(&value, &bits, sizeof(value));
        if (i >= first && (i == first || value > best)) { best = value; winner = i; }
    }
    return (pneu_demo_class_t)(winner+1U);
}

pneu_demo_class_t pneu_model_predict(const float values[12], float positive_peak_counts)
{
    uint16_t words[12];
    float logged[12];
    uint32_t start = board_micros();
    pneu_demo_class_t label;
    unsigned i;
    pneutouch_model.label = pneutouch_model.primary_label = PNEU_CLASS_UNKNOWN;
    pneutouch_model.specialist_used = false;
    pneutouch_model.inference_us = 0;
    if (!pneutouch_model.ready || !logarithms(values, logged) ||
        !normalize(logged, words, model_mean, model_scale)) return PNEU_CLASS_UNKNOWN;
    label = predict_instance(0, words, 0);
    pneutouch_model.primary_label = label;
    for (i = 0; i < 4; ++i) pneutouch_model.primary_scores[i] = pneutouch_model.scores[i];
    if (label == PNEU_CLASS_UNKNOWN) return label;
    if (label == PNEU_CLASS_HEAD || label == PNEU_CLASS_LEGS) {
        if (!(positive_peak_counts > 0.0f && positive_peak_counts < 100000000.0f)) return PNEU_CLASS_UNKNOWN;
        /* Eleven logarithms are shared; only absolute pressure adds a logf. */
        logged[5] = logf(positive_peak_counts/1000.0f);
        if (!normalize(logged, words, specialist_mean, specialist_scale)) return PNEU_CLASS_UNKNOWN;
        pneutouch_model.specialist_used = true;
        label = predict_instance(1, words, 2); /* Only LEGS/HEAD are trained here. */
        if (label == PNEU_CLASS_UNKNOWN) return label;
    }
    pneutouch_model.inference_us = board_micros()-start;
    ++pneutouch_model.predictions;
    pneutouch_model.label = label;
    return pneutouch_model.label;
}

const char *pneu_class_name(pneu_demo_class_t label)
{
    static const char *const names[] = {"UNKNOWN", "TAIL", "BACK", "LEGS", "HEAD"};
    return label >= PNEU_CLASS_TAIL && label <= PNEU_CLASS_HEAD ? names[label] : names[0];
}
