#include "lidsave.h"

#include <stddef.h>

void lidsave_init(LidSave *ls, bool (*save)(void *ctx), void *ctx)
{
    ls->save = save;
    ls->ctx = ctx;
    ls->away = false;
    ls->ok = false;
    ls->back = false;
    ls->saves = 0;
}

void lidsave_event(LidSave *ls, LidSaveEvent e)
{
    if (e == LIDSAVE_SUSPEND || e == LIDSAVE_SLEEP) {
        if (ls->away)
            return;
        ls->ok = ls->save(ls->ctx);
        ls->saves++;
        ls->away = true;
    } else if (e == LIDSAVE_RESTORE || e == LIDSAVE_WAKE) {
        if (!ls->away)
            return;
        ls->away = false;
        ls->back = true;
    }
}

bool lidsave_back(LidSave *ls, bool *ok)
{
    if (!ls->back)
        return false;
    ls->back = false;
    *ok = ls->ok;
    return true;
}
