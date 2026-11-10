#include "ai_validation.h"
#include "board.h"
#include "pressure_model.h"
#include "store_protocol.h"
#include "uart_text.h"
#include "crc16.h"
#include "solistAi.h"
#include "smpl_common.h"
#include "irq.h"
#include <string.h>

#define RX_CAPACITY 600U
#define INPUT_CAPACITY 128U
#define CLASS_COUNT 4U
static char line[RX_CAPACITY];
static unsigned used;
static bool dropping, validation, initialized, live_ready;
static unsigned job; /* 0 idle, 1 training, 2 prediction */
static uint32_t token, started_us, last_command_ms;
static uint16_t input_size;
static bfloat16 input[INPUT_CAPACITY], teacher[CLASS_COUNT], output[CLASS_COUNT];
static bfloat16 reset_row[64];

static void error(const char *why)
{
    pneu_text("# PAI1,ERROR,"); pneu_text(why); pneu_text("\r\n");
}
static void command(void)
{
    char *star = strchr(line, '*'), *fields[8], *p;
    unsigned n = 0, i, j;
    uint16_t received = 0;
    uint32_t id, inputs, hidden, seed, activation, cls;
    if (!star || strlen(star+1) != 4U) { error("CRC_FORMAT"); return; }
    for (i = 1; i <= 4; ++i) {
        int digit = pneu_hex_digit(star[i]);
        if (digit < 0) { error("CRC_FORMAT"); return; }
        received = (uint16_t)((received << 4)|digit);
    }
    if (pneu_crc16(line, (unsigned)(star-line)) != received) { error("CRC"); return; }
    *star = '\0'; p = line; fields[n++] = p;
    while (*p) {
        if (*p == ',') {
            *p = '\0';
            if (n == 8U) { error("FIELDS"); return; }
            fields[n++] = p+1;
        }
        ++p;
    }
    if (!strcmp(fields[0], "PST1")) {
        if (job) { pneu_text("# PST1,ERROR,BUSY\r\n"); return; }
        last_command_ms = board_millis();
        /* TRAIN/TUNE retrain instance 0, so a PAI1 INIT is no longer valid; after TUNE
           pneutouch_model.ready is false, so LIVEPREDICT needs LIVEINIT/TRAIN again. */
        if (pneu_store_command(fields, n, validation)) { initialized = false; live_ready = pneutouch_model.ready; }
        /* TUNE can block longer than the 60 s idle timeout: the idle budget restarts when it ends. */
        last_command_ms = board_millis();
        return;
    }
    if (n < 3 || strcmp(fields[0], "PAI1")) { error("PROTOCOL"); return; }
    if (job) { error("BUSY"); return; }
    last_command_ms = board_millis();
    if (!strcmp(fields[1], "MODE") && n == 3 && pneu_integer(fields[2], &id) && id <= 1) {
        validation = id != 0;
        initialized = live_ready = false;
        pneu_text("# PAI1,MODE,"); pneu_number(id); pneu_text("\r\n");
        return;
    }
    if (!validation) { error("MODE_REQUIRED"); return; }
    if (!pneu_integer(fields[2], &id)) { error("TOKEN"); return; }
    /* Explicit validation only: exercise both boot-trained instances without
       labelling these supplied vectors as physical sensor events. */
    if (!strcmp(fields[1], "LIVEINIT") && n == 3) {
        initialized = false;
        live_ready = pneu_model_init();
        if (!live_ready) { error("LIVE_INIT_FAILED"); return; }
        pneu_text("# PAI1,LIVEINIT,"); pneu_number(id); pneu_text("\r\n");
        return;
    }
    if (!strcmp(fields[1], "LIVEPREDICT") && n == 4) {
        float values[13];
        pneu_demo_class_t label;
        if (!live_ready) { error("LIVE_INIT_REQUIRED"); return; }
        p = fields[3];
        int parsed;
        if (strlen(p) != 13U*8U) { error("INPUT_SIZE"); return; }
        parsed = pneu_parse_f32_hex(p, values, 13);
        if (parsed) { error(parsed == 1 ? "INPUT_HEX" : "NONFINITE"); return; }
        /* Wire format stays 12 features + peak; trough = peak * trough/peak
           ratio (input 10), the same rule tools/feature_select.py uses. */
        label = pneu_model_predict(values, values[12], values[12]*values[10]);
        pneu_text("# PAI1,LIVEPREDICT,"); pneu_number(id); board_putc(','); pneu_number((uint32_t)label);
        /* Field count kept: primary label = label, specialist_used = 0 (single instance). */
        board_putc(','); pneu_number((uint32_t)label); pneu_text(",0");
        board_putc(','); pneu_number(pneutouch_model.inference_us);
        for (i = 0; i < 4; ++i) { board_putc(','); pneu_hex16(pneutouch_model.scores[i]); }
        pneu_text("\r\n");
        return;
    }
    if (!strcmp(fields[1], "INIT") && n == 7 && pneu_integer(fields[3], &inputs) &&
        pneu_integer(fields[4], &hidden) && pneu_integer(fields[5], &seed) && pneu_integer(fields[6], &activation)) {
        ODL_Parameters parameters;
        if ((inputs != 12 && inputs != 64 && inputs != 128) || hidden < 8 || hidden > 64 ||
            seed > 65535 || activation > 3) { error("PARAMETER"); return; }
        memset(&parameters, 0, sizeof(parameters));
        live_ready = false;
        input_size = (uint16_t)inputs;
        parameters.inputSize = input_size; parameters.hiddenSize = (uint16_t)hidden;
        parameters.outputSize = CLASS_COUNT;
        parameters.seed = (uint16_t)seed;
        parameters.forgettingFactor = (bfloat16)0x3f80; /* 1.0 */
        parameters.activationFunction = (uint8_t)activation;
        parameters.lossFunction = ODL_LOSS_MSE;
        parameters.scaleAlpha = (bfloat16)0x3f80; /* fixed +/-1; no tuning on test labels */
        parameters.scaleGamma = 0; parameters.leakRate = (bfloat16)0x3f80;
        irq_ai_dis(); smpl_enablePeripheral(AI_PERI);
        ODL_Initialize(0, &parameters); ODL_Reset(0);
        /* Explicit ridge initialization avoids the legacy library's poorly
           conditioned reset state with only a few independent training inputs.
           Beta=0 and P=I correspond to initial inverse regularization 1. */
        memset(reset_row, 0, sizeof(reset_row));
        for (i = 0; i < hidden; ++i)
            ODL_SetWeightBeta(reset_row, 0, i*CLASS_COUNT*2U, CLASS_COUNT*2U);
        for (i = 0; i < hidden; ++i) {
            if (i) reset_row[i-1] = 0;
            reset_row[i] = (bfloat16)0x3f80;
            ODL_SetWeightP(reset_row, 0, i*hidden*2U, (uint16_t)(hidden*2U));
        }
        irq_ai_clearIRQ();
        initialized = true;
        pneu_text("# PAI1,INIT,"); pneu_number(id); board_putc(','); pneu_number(inputs);
        board_putc(','); pneu_number(hidden); pneu_text(",4\r\n");
        return;
    }
    if (!initialized) { error("INIT_REQUIRED"); return; }
    if (!strcmp(fields[1], "TRAIN") && n == 5 && pneu_integer(fields[3], &cls) && cls >= 1 && cls <= CLASS_COUNT) {
        job = 1; p = fields[4];
    } else if (!strcmp(fields[1], "PREDICT") && n == 4) {
        job = 2; p = fields[3]; cls = 0; /* no test label is sent */
    } else { error("COMMAND"); return; }
    if (strlen(p) != 4U*input_size) { job = 0; error("INPUT_SIZE"); return; }
    for (i = 0; i < input_size; ++i) {
        uint16_t bits = 0;
        for (j = 0; j < 4; ++j) {
            int digit = pneu_hex_digit(*p++);
            if (digit < 0) { job = 0; error("INPUT_HEX"); return; }
            bits = (uint16_t)((bits << 4)|digit);
        }
        if ((bits & 0x7f80U) == 0x7f80U) { job = 0; error("NONFINITE"); return; }
        input[i] = (bfloat16)bits;
    }
    for (i = 0; i < CLASS_COUNT; ++i) teacher[i] = i+1U == cls ? (bfloat16)0x3f80 : 0;
    token = id; started_us = board_micros();
    if (job == 1) ODL_StartTrain(0, input, teacher);
    else ODL_StartPredict(0, input, teacher);
}
void pneu_ai_validation_init(void)
{
    used = job = 0; dropping = validation = initialized = live_ready = false;
}
bool pneu_ai_validation_poll(void)
{
    int c;
    unsigned budget = 64U;
    if (job && !ODL_IsBusy()) {
        uint32_t elapsed = board_micros()-started_us;
        unsigned i;
        pneu_text(job == 1 ? "# PAI1,TRAIN," : "# PAI1,PREDICT,"); pneu_number(token);
        board_putc(','); pneu_number(elapsed);
        if (job == 2) {
            ODL_GetResult(0, output);
            for (i = 0; i < CLASS_COUNT; ++i) { board_putc(','); pneu_hex16((uint16_t)output[i]); }
        }
        pneu_text("\r\n"); job = 0;
    }
    if (job && board_micros()-started_us > 4000000U) {
        job = 0; initialized = false; error("AI_TIMEOUT");
    }
    while (budget-- && (c = board_getc()) >= 0) {
        if (c == '\r') continue;
        if (c == '\n') {
            if (!dropping && used) { line[used] = '\0'; command(); }
            else if (dropping) error("OVERLONG");
            used = 0; dropping = false;
        } else if (!dropping) {
            if (used+1U < sizeof(line)) line[used++] = (char)c;
            else { used = 0; dropping = true; }
        }
    }
    if (validation && !job && board_millis()-last_command_ms > 60000U) {
        validation = initialized = live_ready = false; pneu_text("# PAI1,MODE,0,IDLE_TIMEOUT\r\n");
    }
    return validation;
}
