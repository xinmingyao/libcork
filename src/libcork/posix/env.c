/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2013, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libcork/core.h"
#include "libcork/ds.h"
#include "libcork/os/subprocess.h"
#include "libcork/helpers/errors.h"

extern const char  **environ;


struct cork_env_var {
    const char  *name;
    const char  *value;
};

static struct cork_env_var *
cork_env_var_new(const char *name, const char *value)
{
    struct cork_env_var  *var = cork_new(struct cork_env_var);
    var->name = cork_strdup(name);
    var->value = cork_strdup(value);
    return var;
}

static void
cork_env_var_free(struct cork_env_var *var)
{
    cork_strfree(var->name);
    cork_strfree(var->value);
    free(var);
}


struct cork_env {
    struct cork_hash_table  variables;
    struct cork_buffer  buffer;
};

struct cork_env *
cork_env_new(void)
{
    struct cork_env  *env = cork_new(struct cork_env);
    cork_string_hash_table_init(&env->variables, 0);
    cork_buffer_init(&env->buffer);
    return env;
}

static void
cork_env_add_internal(struct cork_env *env, const char *name, const char *value)
{
    struct cork_env_var  *var = cork_env_var_new(name, value);
    void  *old_var;

    cork_hash_table_put
        (&env->variables, (void *) var->name, var, NULL, NULL, &old_var);

    if (old_var != NULL) {
        cork_env_var_free(old_var);
    }
}

struct cork_env *
cork_env_clone_current(void)
{
    const char  **curr;
    struct cork_env  *env = cork_env_new();

    for (curr = environ; *curr != NULL; curr++) {
        const char  *entry = *curr;
        const char  *equal;

        equal = strchr(entry, '=');
        if (CORK_UNLIKELY(equal == NULL)) {
            /* This environment entry is malformed; skip it. */
            continue;
        }

        /* Make a copy of the name so that it's NUL-terminated rather than
         * equal-terminated. */
        cork_buffer_set(&env->buffer, entry, equal - entry);
        cork_env_add_internal(env, env->buffer.buf, equal + 1);
    }

    return env;
}


static enum cork_hash_table_map_result
cork_env_free_vars(struct cork_hash_table_entry *entry, void *user_data)
{
    struct cork_env_var  *var = entry->value;
    cork_env_var_free(var);
    return CORK_HASH_TABLE_MAP_DELETE;
}

void
cork_env_free(struct cork_env *env)
{
    cork_hash_table_map(&env->variables, cork_env_free_vars, NULL);
    cork_hash_table_done(&env->variables);
    cork_buffer_done(&env->buffer);
    free(env);
}

void
cork_env_add(struct cork_env *env, const char *name, const char *value)
{
    cork_env_add_internal(env, name, value);
}

void
cork_env_add_vprintf(struct cork_env *env, const char *name,
                     const char *format, va_list args)
{
    cork_buffer_vprintf(&env->buffer, format, args);
    cork_env_add_internal(env, name, env->buffer.buf);
}

void
cork_env_add_printf(struct cork_env *env, const char *name,
                    const char *format, ...)
{
    va_list  args;
    va_start(args, format);
    cork_env_add_vprintf(env, name, format, args);
    va_end(args);
}

void
cork_env_remove(struct cork_env *env, const char *name)
{
    void  *old_var;
    cork_hash_table_delete(&env->variables, (void *) name, NULL, &old_var);
    if (old_var != NULL) {
        cork_env_var_free(old_var);
    }
}


static enum cork_hash_table_map_result
cork_env_set_vars(struct cork_hash_table_entry *entry, void *user_data)
{
    struct cork_env_var  *var = entry->value;
    setenv(var->name, var->value, false);
    return CORK_HASH_TABLE_MAP_CONTINUE;
}

void
cork_env_replace_current(struct cork_env *env)
{
    clearenv();
    cork_hash_table_map(&env->variables, cork_env_set_vars, NULL);
}