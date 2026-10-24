/* DINO (4 zones) and KRAG (2 zones) profiles; see object_profile.h. */
#include "object_profile.h"
#include <string.h>

#define SLOT_NAME 8U

static const pneu_object_profile_t profiles[PNEU_OBJECT_COUNT] = {
    {"DINO", 4U, {"TAIL", "BACK", "LEGS", "HEAD"}},
    {"KRAG", 2U, {"HAT", "FACE", "", ""}},
};

const pneu_object_profile_t *pneu_object(pneu_object_t object)
{
    return &profiles[(unsigned)object < PNEU_OBJECT_COUNT ? (unsigned)object : 0U];
}

pneu_object_t pneu_object_of_name(const char *name)
{
    unsigned i;
    for (i = 0; name && i < PNEU_OBJECT_COUNT; ++i)
        if (!strncmp(name, profiles[i].tag, PNEU_OBJECT_TAG)) return (pneu_object_t)i;
    return PNEU_OBJECT_DINO;
}

uint8_t pneu_object_class_mask(pneu_object_t object)
{
    return (uint8_t)((1U << pneu_object(object)->classes)-1U);
}

const char *pneu_object_class_name(pneu_object_t object, uint8_t label)
{
    const pneu_object_profile_t *profile = pneu_object(object);
    return label >= 1U && label <= profile->classes ? profile->name[label-1U] : "UNKNOWN";
}

const pneu_detect_t *pneu_object_detect(pneu_object_t object)
{
    return object == PNEU_OBJECT_KRAG ? &PNEU_DETECT_KRAG : &PNEU_DETECT_DINO;
}

void pneu_object_slot_name(pneu_object_t object, const char *suffix, char out[9])
{
    unsigned at = PNEU_OBJECT_TAG, i;
    memcpy(out, pneu_object(object)->tag, PNEU_OBJECT_TAG);
    for (i = 0; suffix && suffix[i] && at < SLOT_NAME; ++i) out[at++] = suffix[i];
    out[at] = '\0';
}
