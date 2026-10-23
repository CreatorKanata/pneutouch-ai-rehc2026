/* PST1 UART commands for the FeRAM model store (docs/model-store.md). */
#ifndef PNEU_STORE_PROTOCOL_H
#define PNEU_STORE_PROTOCOL_H
#include <stdbool.h>
/* fields[0] == "PST1"; validation = PAI1 MODE 1 (acquisition paused).
   Returns true when the Solist-AI instances were retrained (TRAIN). */
bool pneu_store_command(char *const fields[], unsigned n, bool validation);
#endif
