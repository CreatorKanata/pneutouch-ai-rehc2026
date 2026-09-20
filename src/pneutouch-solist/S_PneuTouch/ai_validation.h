/* Explicit PC-driven validation mode; training/prediction execute on Solist-AI. */
#ifndef PNEU_AI_VALIDATION_H
#define PNEU_AI_VALIDATION_H
#include <stdbool.h>
void pneu_ai_validation_init(void);
bool pneu_ai_validation_poll(void);
#endif
