#include <stdio.h>
#define const // problem? :trollface: (we want the wl_interfaces below to not be defined as const)
#include <wayland-client.h>
#undef const

#define WL_CLOSURE_MAX_ARGS 20

struct wl_interface wl_pointer_interface;
struct wl_interface wl_output_interface;
struct wl_interface wl_display_interface;
struct wl_interface wl_shell_interface;
struct wl_interface wl_shell_surface_interface;
struct wl_interface wl_touch_interface;
struct wl_interface wl_subsurface_interface;
struct wl_interface wl_subcompositor_interface;
struct wl_interface wl_shm_interface;
struct wl_interface wl_shm_pool_interface;
struct wl_interface wl_compositor_interface;
struct wl_interface wl_seat_interface;
struct wl_interface wl_buffer_interface;
struct wl_interface wl_registry_interface;
struct wl_interface wl_surface_interface;
struct wl_interface wl_keyboard_interface;
struct wl_interface wl_callback_interface;
struct wl_interface wl_region_interface;
struct wl_interface wl_data_device_interface;
struct wl_interface wl_data_source_interface;
struct wl_interface wl_data_offer_interface;
struct wl_interface wl_data_device_manager_interface;

static const char* get_next_argument_type(const char* signature, char* type) {
    for (; *signature; ++signature) {
        switch (*signature) {
        case 'i':
        case 'u':
        case 'f':
        case 's':
        case 'o':
        case 'n':
        case 'a':
        case 'h':
            *type = *signature;
            return signature + 1;
        case '?':
            break;
        }
    }
    *type = 0;
    return signature;
}

static void va_list_to_args(const char* signature, union wl_argument* args, va_list list) {
    const char* iterator = signature;
    for (int i = 0; i < WL_CLOSURE_MAX_ARGS; i++) {
        char arg_type;
        iterator = get_next_argument_type(iterator, &arg_type);
        switch (arg_type) {
        case 'i':
            args[i].i = va_arg(list, int32_t);
            break;
        case 'u':
            args[i].u = va_arg(list, uint32_t);
            break;
        case 'f':
            args[i].f = va_arg(list, wl_fixed_t);
            break;
        case 's':
            args[i].s = va_arg(list, char*);
            break;
        case 'o':
            args[i].o = va_arg(list, struct wl_object*);
            break;
        case 'n':
            args[i].o = va_arg(list, struct wl_object*);
            break;
        case 'a':
            args[i].a = va_arg(list, struct wl_array*);
            break;
        case 'h':
            args[i].h = va_arg(list, int32_t);
            break;
        case 0:
            return;
        default:
            continue;
        }
    }
}

// We deal with the variadic arguments in the guest side since it's way easier than marshalling them to host
void wl_proxy_marshal(struct wl_proxy* proxy, uint32_t opcode, ...) {
    const char* signature = (*(struct wl_interface**)proxy)->methods[opcode].signature;

    union wl_argument args[WL_CLOSURE_MAX_ARGS];
    va_list list;
    va_start(list, opcode);
    va_list_to_args(signature, args, list);
    va_end(list);

    wl_proxy_marshal_array(proxy, opcode, args);
}

struct wl_proxy* wl_proxy_marshal_constructor(struct wl_proxy* proxy, uint32_t opcode, struct wl_interface* interface, ...) {
    const char* signature = (*(struct wl_interface**)proxy)->methods[opcode].signature;

    union wl_argument args[WL_CLOSURE_MAX_ARGS];
    va_list list;
    va_start(list, interface);
    va_list_to_args(signature, args, list);
    va_end(list);

    return wl_proxy_marshal_array_constructor(proxy, opcode, args, interface);
}

struct wl_proxy* wl_proxy_marshal_constructor_versioned(struct wl_proxy* proxy, uint32_t opcode, struct wl_interface* interface, uint32_t version,
                                                        ...) {
    const char* signature = (*(struct wl_interface**)proxy)->methods[opcode].signature;

    union wl_argument args[WL_CLOSURE_MAX_ARGS];
    va_list list;
    va_start(list, version);
    va_list_to_args(signature, args, list);
    va_end(list);

    return wl_proxy_marshal_array_constructor_versioned(proxy, opcode, args, interface, version);
}

struct wl_proxy* wl_proxy_marshal_flags(struct wl_proxy* proxy, uint32_t opcode, struct wl_interface* interface, uint32_t version, uint32_t flags,
                                        ...) {
    const char* signature = (*(struct wl_interface**)proxy)->methods[opcode].signature;

    union wl_argument args[WL_CLOSURE_MAX_ARGS];
    va_list list;
    va_start(list, flags);
    va_list_to_args(signature, args, list);
    va_end(list);

    return wl_proxy_marshal_array_flags(proxy, opcode, interface, version, flags, args);
}