/*
 * QEMU Host Memory Backend
 *
 * Copyright (C) 2013 Red Hat Inc
 *
 * Authors:
 *   Igor Mammedov <imammedo@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "sysemu/hostmem.h"

/* hostmem-file.c */
/**
 * @TYPE_MEMORY_BACKEND_FILE:
 * name of backend that uses mmap on a file descriptor
 */
#define TYPE_MEMORY_BACKEND_FILE "memory-file"

#define MEMORY_BACKEND_FILE(obj) \
    OBJECT_CHECK(HostMemoryBackendFile, (obj), TYPE_MEMORY_BACKEND_FILE)

typedef struct HostMemoryBackendFile HostMemoryBackendFile;

struct HostMemoryBackendFile {
    HostMemoryBackend parent_obj;
    char *mem_path;
};

static void
file_backend_memory_init(HostMemoryBackend *backend, Error **errp)
{
    HostMemoryBackendFile *fb = MEMORY_BACKEND_FILE(backend);
    if (!fb->mem_path) {
        error_setg(errp, "mem-path property not set\n");
        return;
    }
#ifndef CONFIG_LINUX
    error_setg(errp, "-mem-path not supported on this host\n");
#else
    if (!memory_region_size(&backend->mr)) {
        memory_region_init_ram_from_file(&backend->mr, OBJECT(backend),
                                         object_get_canonical_path(OBJECT(backend)),
                                         backend->size,
                                         fb->mem_path);
    }
#endif
}

static void
file_backend_class_init(ObjectClass *oc, void *data)
{
    HostMemoryBackendClass *bc = MEMORY_BACKEND_CLASS(oc);

    bc->memory_init = file_backend_memory_init;
}

static char *get_mem_path(Object *o, Error **errp)
{
    HostMemoryBackendFile *fb = MEMORY_BACKEND_FILE(o);

    return g_strdup(fb->mem_path);
}

static void set_mem_path(Object *o, const char *str, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(o);
    HostMemoryBackendFile *fb = MEMORY_BACKEND_FILE(o);

    if (memory_region_size(&backend->mr)) {
        error_setg(errp, "cannot change property value\n");
        return;
    }
    if (fb->mem_path) {
        g_free(fb->mem_path);
    }
    fb->mem_path = g_strdup(str);
}

static void
file_backend_instance_init(Object *o)
{
    object_property_add_str(o, "mem-path", get_mem_path,
                            set_mem_path, NULL);
}

static const TypeInfo file_backend_info = {
    .name = TYPE_MEMORY_BACKEND_FILE,
    .parent = TYPE_MEMORY_BACKEND,
    .class_init = file_backend_class_init,
    .instance_init = file_backend_instance_init,
    .instance_size = sizeof(HostMemoryBackendFile),
};

static void register_types(void)
{
    type_register_static(&file_backend_info);
}

type_init(register_types);
