#include "rg_system.h"
#include "translations.h"

static int rg_language = RG_LANG_ZH;

int rg_localization_get_language_id(void)
{
    return rg_language;
}

bool rg_localization_set_language_id(int language_id)
{
    if (language_id < 0 || language_id > RG_LANG_MAX - 1)
        return false;

    rg_language = language_id;
    return true;
}

const char *rg_gettext(const char *text)
{
    if (rg_language == RG_LANG_EN || text == NULL)
        return text;

    for (size_t i = 0; i < RG_COUNT(translations); ++i)
    {
        if (strcmp(translations[i][RG_LANG_EN], text) == 0)
        {
            const char *msg = translations[i][rg_language];
            return msg ? msg : text;
        }
    }

    return text;
}

const char *rg_localization_get_language_name(int language_id)
{
    if (language_id < 0 || language_id > RG_LANG_MAX - 1)
        return NULL;
    return language_names[language_id];
}