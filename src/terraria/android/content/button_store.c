#include "button_store.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUTTON_STORE_LINE_SIZE 4096

static const char *store_name = "controls_bindings.dat";

static bool make_path(char *output, size_t output_size, const char *directory,
                      const char *suffix) {
    if (!output || output_size == 0 || !directory || !suffix) return false;
    const int written = snprintf(output, output_size, "%s/%s%s", directory,
                                 store_name, suffix);
    return written > 0 && (size_t)written < output_size;
}

static bool encode(FILE *file, const char *value) {
    static const char hex[] = "0123456789ABCDEF";
    if (!file || !value) return false;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        const unsigned char c = *p;
        if (c == '%' || c == '\t' || c == '\n' || c == '\r') {
            if (fputc('%', file) == EOF || fputc(hex[c >> 4], file) == EOF ||
                fputc(hex[c & 15], file) == EOF) {
                return false;
            }
        } else if (fputc(c, file) == EOF) {
            return false;
        }
    }
    return true;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool decode(const char *input, char *output, size_t output_size) {
    if (!input || !output || output_size == 0) return false;
    size_t used = 0;
    while (*input) {
        unsigned char value = (unsigned char)*input++;
        if (value == '%') {
            const int high = hex_value(input[0]);
            const int low = input[0] && input[1] ? hex_value(input[1]) : -1;
            if (high < 0 || low < 0) return false;
            value = (unsigned char)((high << 4) | low);
            input += 2;
        }
        if (value == '\0' || value == '\n' || value == '\r' ||
            used + 1 >= output_size) {
            return false;
        }
        output[used++] = (char)value;
    }
    output[used] = '\0';
    return true;
}

static bool write_field(FILE *file, const char *value, bool final) {
    if (!encode(file, value)) return false;
    return fputc(final ? '\n' : '\t', file) != EOF;
}

static bool replace_file(const char *temporary, const char *path) {
    return rename(temporary, path) == 0;
}

bool button_store_save(const button_state_t *state, const char *directory) {
    if (!state || !directory || *directory == '\0') return false;

    char path[BUTTON_STORE_LINE_SIZE];
    char temporary[BUTTON_STORE_LINE_SIZE];
    if (!make_path(path, sizeof(path), directory, "") ||
        !make_path(temporary, sizeof(temporary), directory, ".tmp")) {
        return false;
    }

    FILE *file = fopen(temporary, "wb");
    if (!file) return false;

    bool ok = fputs("button-bindings\t1\n", file) >= 0;
    for (size_t i = 0; ok && i < state->bindings.count; ++i) {
        const button_binding_t *binding = &state->bindings.items[i];
        ok = fputs("binding\t", file) >= 0 &&
             write_field(file, binding->profile, false) &&
             write_field(file, binding->slot, false) &&
             write_field(file, binding->key, true);
    }

    if (fflush(file) != 0) ok = false;
    if (ok && fsync(fileno(file)) != 0) ok = false;
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        remove(temporary);
        return false;
    }

    if (!replace_file(temporary, path)) {
        remove(temporary);
        return false;
    }
    return true;
}

bool button_store_load(button_state_t *state, const char *directory) {
    if (!state || !directory || *directory == '\0') return false;

    char path[BUTTON_STORE_LINE_SIZE];
    if (!make_path(path, sizeof(path), directory, "")) return false;

    FILE *file = fopen(path, "rb");
    if (!file) return errno == ENOENT;

    char line[BUTTON_STORE_LINE_SIZE];
    if (!fgets(line, sizeof(line), file) ||
        strcmp(line, "button-bindings\t1\n") != 0) {
        fclose(file);
        return false;
    }

    button_state_t parsed;
    button_state_init(&parsed);
    bool ok = true;
    while (ok && fgets(line, sizeof(line), file)) {
        const size_t length = strlen(line);
        if (length == 0 || line[length - 1] != '\n') {
            ok = false;
            break;
        }

        char *fields[4] = {0};
        size_t field_count = 0;
        char *cursor = line;
        while (field_count < 4) {
            fields[field_count++] = cursor;
            char *tab = strchr(cursor, '\t');
            char *newline = strchr(cursor, '\n');
            if (tab && (!newline || tab < newline)) {
                *tab = '\0';
                cursor = tab + 1;
                continue;
            }
            if (newline) *newline = '\0';
            break;
        }

        char profile[1024];
        char slot[1024];
        char key[1024];
        if (field_count != 4 || strcmp(fields[0], "binding") != 0 ||
            !decode(fields[1], profile, sizeof(profile)) ||
            !decode(fields[2], slot, sizeof(slot)) ||
            !decode(fields[3], key, sizeof(key)) ||
            !button_state_add_binding(&parsed, profile, slot, key)) {
            ok = false;
        }
    }
    if (ferror(file)) ok = false;
    fclose(file);

    if (ok) {
        button_state_replace_bindings(state, &parsed.bindings);
    }
    button_state_destroy(&parsed);
    return ok;
}
