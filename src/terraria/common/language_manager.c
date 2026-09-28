/*******************************************************************************
 * tefkernel - language_manager
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2026/9/16
 *******************************************************************************/

#include "internal/terraria/language_manager.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/string.h"
#include "tefstd/vector.h"
#include <stddef.h>

static tefstd_vector_t g_callback;

static patch_handle_t localized_text_ctor =
    PATCH_NULL; // LocalizedText(string key, string text)
static patch_handle_t localized_text_type = PATCH_NULL;

static patch_handle_t language_get_text =
    PATCH_NULL; // static LocalizedText GetText(string key)

static patch_handle_t load_text_json =
    PATCH_NULL; // void LoadLanguageFromFileText(string fileText)
// void LoadLanguageFromFileTextJson(string fileText, bool canCreateCategories)

static patch_handle_t process_copy_commands_in_texts =
    PATCH_NULL; // void ProcessCopyCommandsInTexts()

static patch_handle_t legacy_id_f = PATCH_NULL;

static void load_language_postfix(patch_handle_t instance, void **args,
                                  void *result,
                                  const patch_method_signature_t *sig_info) {
  if (!instance || !args[0])
    return;

  if (!patchlib_is_valid(*(patch_handle_t *)args[0]))
    return;

  int legacy_id = 0;
  patchlib_field_get_value(legacy_id_f, *(patch_handle_t *)args[0], &legacy_id);

  for (int i = 0; i < tefstd_vector_size(&g_callback); ++i) {
    terraria_languager_manager_get_localized_text_t callback =
        *(terraria_languager_manager_get_localized_text_t *)tefstd_vector_at(
            &g_callback, i);

    const char *callback_text = callback((language_culture_t)legacy_id);
#if __ANDROID__
    if (callback_text) {
      void (*load_content)(void *, void *) = ((
          void (*)(void *, void *))patchlib_method_get_pointer(load_text_json));
      load_content(instance, patchlib_string_create(callback_text));
    }
#else
    if (callback_text) {
      patch_handle_t content =
          patchlib_string_create(callback((language_culture_t)legacy_id));
      bool can_create_categories = true;
      void *iargs[2] = {&content, &can_create_categories};
      patchlib_method_invoke_args(load_text_json, instance, NULL, iargs);

      patchlib_free(content);
    }
#endif
  }

#if __ANDROID__
  ((void (*)(void *))patchlib_method_get_pointer(
      process_copy_commands_in_texts))(instance);
#else
  patchlib_method_invoke_args(process_copy_commands_in_texts, instance, NULL,
                              NULL);
#endif
}

void terraria_language_manager_init() {
  tefstd_vector_init(&g_callback,
                     sizeof(terraria_languager_manager_get_localized_text_t));

  localized_text_type =
      patchlib_type_get_type("Terraria.Localization", "LocalizedText");
  localized_text_ctor =
      patchlib_type_get_method_by_param_count(localized_text_type, ".ctor", 2);

  patch_handle_t language_type =
      patchlib_type_get_type("Terraria.Localization", "Language");
  language_get_text =
      patchlib_type_get_method_by_param_count(language_type, "GetText", 1);

  patch_handle_t language_manager_type =
      patchlib_type_get_type("Terraria.Localization", "LanguageManager");

  load_text_json =
#if __ANDROID__
      patchlib_type_get_method_by_param_count(language_manager_type,
                                              "LoadLanguageFromFileText", 1);
#else
      patchlib_type_get_method_by_param_count(
          language_manager_type, "LoadLanguageFromFileTextJson", 2);
#endif
  process_copy_commands_in_texts = patchlib_type_get_method_by_param_count(
      language_manager_type, "ProcessCopyCommandsInTexts", 0);

  patch_handle_t load_language =
      patchlib_type_get_method(language_manager_type, "LoadLanguage");
  patchlib_install_prepost_hook(load_language, NULL, load_language_postfix);

  patch_handle_t game_culture =
      patchlib_type_get_type("Terraria.Localization", "GameCulture");
  legacy_id_f = patchlib_type_get_field(game_culture, "LegacyId");

  patchlib_free(game_culture);
  patchlib_free(localized_text_type);
  patchlib_free(language_type);
  patchlib_free(language_manager_type);
  patchlib_free(load_language);
}

patch_handle_t
terraria_language_manager_create_localized_text(const char *key,
                                                const char *value) {

#if __ANDROID__
  patch_handle_t localized_text =
      patchlib_type_new_instance(localized_text_type);
  ((void (*)(void *, void *, void *))patchlib_method_get_pointer(
      localized_text_ctor))(localized_text, patchlib_string_create(key),
                            patchlib_string_create(value));
  return localized_text;
#else

  patch_handle_t localized_text = PATCH_NULL;
  patch_handle_t nkey = patchlib_string_create(key);
  patch_handle_t nvalue = patchlib_string_create(value);
  void *args[2] = {&nkey, &nvalue};
  patchlib_constructor_invoke(localized_text_ctor, &localized_text, args);

  patchlib_free(nkey);
  patchlib_free(nvalue);

  return localized_text;
#endif
}

patch_handle_t terraria_language_manager_get_localized_text(const char *key) {
#if __ANDROID__
  return ((void *(*)(void *))patchlib_method_get_pointer(language_get_text))(
      patchlib_string_create(key));
#else
  patch_handle_t nkey = patchlib_string_create(key);
  patch_handle_t localized_text = PATCH_NULL;
  void *args[1] = {&nkey};

patchlib_method_invoke_args(language_get_text, NULL,
                                                      &localized_text, args);
  patchlib_free(nkey);
  return localized_text;
#endif
}

void terraria_language_manager_register_callback(
    terraria_languager_manager_get_localized_text_t callback) {
  tefstd_vector_push_back(&g_callback, &callback);
}
