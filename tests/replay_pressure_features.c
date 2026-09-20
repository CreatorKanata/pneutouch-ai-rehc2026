/* Host-only harness. Firmware uses the exact same pressure_features.c. */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "pressure_features.h"

static pneu_features_t state;
int main(int argc, char **argv)
{
    FILE *input;
    char line[512];
    if (argc != 2 || !(input = fopen(argv[1], "r"))) return 2;
    pneu_features_init(&state);
    while (fgets(line, sizeof(line), input)) {
        uint32_t seq, ms;
        int32_t raw;
        char *p = strchr(line, ',');
        unsigned i;
        pneu_event_result_t result;
        if (!p || sscanf(p+1, "%" SCNu32 ",%" SCNu32 ",%" SCNd32, &seq, &ms, &raw) != 3) continue;
        result = pneu_features_feed(&state, seq, ms, raw);
        if (result == PNEU_EVENT_NONE || result == PNEU_EVENT_RESET) continue;
        printf("%u,%" PRIu32 ",%" PRIu32 ",%" PRIu32, (unsigned)result, state.event.id, state.event.trigger_ms, ms);
        if (result == PNEU_EVENT_COMPLETE) {
            for (i = 0; i < PNEU_FEATURE_COUNT; ++i) printf(",%.8g", (double)state.event.value[i]);
            printf(",%.8g,%.8g,%.8g", (double)state.event.baseline, (double)state.event.peak, (double)state.event.trough);
        }
        putchar('\n');
    }
    fclose(input);
    return 0;
}
