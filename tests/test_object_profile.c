/* object_profile.c: tags, class masks, zone names, object of a slot name, slot names. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "object_profile.h"

static void test_profiles(void)
{
    assert(!strcmp(pneu_object(PNEU_OBJECT_DINO)->tag, "DINO") && pneu_object(PNEU_OBJECT_DINO)->classes == 4);
    assert(!strcmp(pneu_object(PNEU_OBJECT_KRAG)->tag, "KRAG") && pneu_object(PNEU_OBJECT_KRAG)->classes == 2);
    assert(pneu_object((pneu_object_t)7) == pneu_object(PNEU_OBJECT_DINO));
    assert(pneu_object_class_mask(PNEU_OBJECT_DINO) == 0x0F && pneu_object_class_mask(PNEU_OBJECT_KRAG) == 0x03);
}

static void test_names(void)
{
    static const char *const dino[] = {"UNKNOWN", "TAIL", "BACK", "LEGS", "HEAD", "UNKNOWN"};
    static const char *const krag[] = {"UNKNOWN", "HAT", "FACE", "UNKNOWN", "UNKNOWN", "UNKNOWN"};
    for (uint8_t label = 0; label < 6; ++label) {
        assert(!strcmp(pneu_object_class_name(PNEU_OBJECT_DINO, label), dino[label]));
        assert(!strcmp(pneu_object_class_name(PNEU_OBJECT_KRAG, label), krag[label]));
    }
    assert(!strcmp(pneu_object_class_name(PNEU_OBJECT_KRAG, 255), "UNKNOWN"));
}

static void test_object_of_name(void)
{
    assert(pneu_object_of_name("DINO1023") == PNEU_OBJECT_DINO);
    assert(pneu_object_of_name("KRAGBTN") == PNEU_OBJECT_KRAG && pneu_object_of_name("KRAG") == PNEU_OBJECT_KRAG);
    assert(pneu_object_of_name("KRAGx") == PNEU_OBJECT_KRAG);
    assert(pneu_object_of_name("1023-176") == PNEU_OBJECT_DINO && pneu_object_of_name("BTN") == PNEU_OBJECT_DINO);
    assert(pneu_object_of_name("") == PNEU_OBJECT_DINO && pneu_object_of_name(NULL) == PNEU_OBJECT_DINO);
    assert(pneu_object_of_name("krag") == PNEU_OBJECT_DINO && pneu_object_of_name("KRA") == PNEU_OBJECT_DINO);
}

static void test_slot_name(void)
{
    char name[9];
    pneu_object_slot_name(PNEU_OBJECT_KRAG, "BTN", name); assert(!strcmp(name, "KRAGBTN"));
    pneu_object_slot_name(PNEU_OBJECT_DINO, "BTN", name); assert(!strcmp(name, "DINOBTN"));
    pneu_object_slot_name(PNEU_OBJECT_KRAG, "1024", name); assert(!strcmp(name, "KRAG1024"));
    pneu_object_slot_name(PNEU_OBJECT_KRAG, "123456", name); assert(!strcmp(name, "KRAG1234"));
    pneu_object_slot_name(PNEU_OBJECT_DINO, "", name); assert(!strcmp(name, "DINO"));
    pneu_object_slot_name(PNEU_OBJECT_KRAG, NULL, name); assert(!strcmp(name, "KRAG"));
}

int main(void)
{
    test_profiles();
    test_names();
    test_object_of_name();
    test_slot_name();
    puts("Object profile: DINO/KRAG tags, class masks, zone names, object of a slot name, slot name truncation PASS");
    return 0;
}
