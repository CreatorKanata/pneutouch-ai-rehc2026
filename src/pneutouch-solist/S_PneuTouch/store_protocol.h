/* PST1 UART commands for the FeRAM model store (docs/model-store.md). */
#ifndef PNEU_STORE_PROTOCOL_H
#define PNEU_STORE_PROTOCOL_H
#include <stdbool.h>
/* fields[0] == "PST1"; validation = PAI1 MODE 1 (acquisition paused).
   Returns true when Solist-AI instance 0 was retrained (TRAIN) or clobbered (TUNE). */
bool pneu_store_command(char *const fields[], unsigned n, bool validation);
/* The PST1 TUNE reply after the id: ",<ok>,<err>,..." up to and including ",<us>",
   no line end (also used by the SW2 "# PNET1," line). */
void pneu_store_tune_fields(bool ok);
#endif
