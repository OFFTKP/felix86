bits 64

section .data

extern wl_output_interface
extern wl_shell_interface
extern wl_shell_surface_interface
extern wl_display_interface
extern wl_touch_interface
extern wl_subsurface_interface
extern wl_subcompositor_interface
extern wl_shm_pool_interface
extern wl_pointer_interface
extern wl_compositor_interface
extern wl_shm_interface
extern wl_registry_interface
extern wl_buffer_interface
extern wl_seat_interface
extern wl_surface_interface
extern wl_keyboard_interface
extern wl_callback_interface
extern wl_region_interface
extern wl_data_device_interface
extern wl_data_source_interface
extern wl_data_offer_interface
extern wl_data_device_manager_interface

wl_display_interface_name:
db "wl_display_interface", 0
wl_output_interface_name:
db "wl_output_interface", 0
wl_shell_interface_name:
db "wl_shell_interface", 0
wl_shell_surface_interface_name:
db "wl_shell_surface_interface", 0
wl_touch_interface_name:
db "wl_touch_interface", 0
wl_subsurface_interface_name:
db "wl_subsurface_interface", 0
wl_subcompositor_interface_name:
db "wl_subcompositor_interface", 0
wl_shm_pool_interface_name:
db "wl_shm_pool_interface", 0
wl_pointer_interface_name:
db "wl_pointer_interface", 0
wl_compositor_interface_name:
db "wl_compositor_interface", 0
wl_shm_interface_name:
db "wl_shm_interface", 0
wl_registry_interface_name:
db "wl_registry_interface", 0
wl_buffer_interface_name:
db "wl_buffer_interface", 0
wl_seat_interface_name:
db "wl_seat_interface", 0
wl_surface_interface_name:
db "wl_surface_interface", 0
wl_keyboard_interface_name:
db "wl_keyboard_interface", 0
wl_callback_interface_name:
db "wl_callback_interface", 0
wl_region_interface_name:
db "wl_region_interface", 0
wl_data_device_interface_name:
db "wl_data_device_interface", 0
wl_data_source_interface_name:
db "wl_data_source_interface", 0
wl_data_offer_interface_name:
db "wl_data_offer_interface", 0
wl_data_device_manager_interface_name:
db "wl_data_device_manager_interface", 0

libname:
db "libwayland-client.so", 0

section .text

global __felix86_constructor:function
align 16
__felix86_constructor:
invlpg [rbx]
ret
dd 0x12345678 ; invlpg + ret are 4 bytes, four more here to align to pointer
dq libname
; the constructor will set these to the host libwayland-client pointers
dq wl_output_interface_name
dq wl_output_interface
dq wl_display_interface_name
dq wl_display_interface
dq wl_shell_interface_name
dq wl_shell_interface
dq wl_shell_surface_interface_name
dq wl_shell_surface_interface
dq wl_touch_interface_name
dq wl_touch_interface
dq wl_subsurface_interface_name
dq wl_subsurface_interface
dq wl_subcompositor_interface_name
dq wl_subcompositor_interface
dq wl_shm_pool_interface_name
dq wl_shm_pool_interface
dq wl_pointer_interface_name
dq wl_pointer_interface
dq wl_compositor_interface_name
dq wl_compositor_interface
dq wl_shm_interface_name
dq wl_shm_interface
dq wl_registry_interface_name
dq wl_registry_interface
dq wl_buffer_interface_name
dq wl_buffer_interface
dq wl_seat_interface_name
dq wl_seat_interface
dq wl_surface_interface_name
dq wl_surface_interface
dq wl_keyboard_interface_name
dq wl_keyboard_interface
dq wl_callback_interface_name
dq wl_callback_interface
dq wl_region_interface_name
dq wl_region_interface
dq wl_data_device_interface_name
dq wl_data_device_interface
dq wl_data_source_interface_name
dq wl_data_source_interface
dq wl_data_offer_interface_name
dq wl_data_offer_interface
dq wl_data_device_manager_interface_name
dq wl_data_device_manager_interface
dq 0
dq 0

global wl_display_connect:function
align 16
wl_display_connect:
invlpg [rax]
db "wl_display_connect", 0
ret

global wl_display_flush:function
align 16
wl_display_flush:
invlpg [rax]
db "wl_display_flush", 0
ret

global wl_display_cancel_read:function
align 16
wl_display_cancel_read:
invlpg [rax]
db "wl_display_cancel_read", 0
ret

global wl_display_create_queue:function
align 16
wl_display_create_queue:
invlpg [rax]
db "wl_display_create_queue", 0
ret

global wl_display_disconnect:function
align 16
wl_display_disconnect:
invlpg [rax]
db "wl_display_disconnect", 0
ret

global wl_display_dispatch:function
align 16
wl_display_dispatch:
invlpg [rax]
db "wl_display_dispatch", 0
ret

global wl_display_dispatch_pending:function
align 16
wl_display_dispatch_pending:
invlpg [rax]
db "wl_display_dispatch_pending", 0
ret

global wl_display_dispatch_queue:function
align 16
wl_display_dispatch_queue:
invlpg [rax]
db "wl_display_dispatch_queue", 0
ret

global wl_display_dispatch_queue_pending:function
align 16
wl_display_dispatch_queue_pending:
invlpg [rax]
db "wl_display_dispatch_queue_pending", 0
ret

global wl_display_get_error:function
align 16
wl_display_get_error:
invlpg [rax]
db "wl_display_get_error", 0
ret

global wl_display_prepare_read:function
align 16
wl_display_prepare_read:
invlpg [rax]
db "wl_display_prepare_read", 0
ret

global wl_display_prepare_read_queue:function
align 16
wl_display_prepare_read_queue:
invlpg [rax]
db "wl_display_prepare_read_queue", 0
ret

global wl_display_read_events:function
align 16
wl_display_read_events:
invlpg [rax]
db "wl_display_read_events", 0
ret

global wl_display_roundtrip:function
align 16
wl_display_roundtrip:
invlpg [rax]
db "wl_display_roundtrip", 0
ret

global wl_display_roundtrip_queue:function
align 16
wl_display_roundtrip_queue:
invlpg [rax]
db "wl_display_roundtrip_queue", 0
ret

global wl_display_connect_to_fd:function
align 16
wl_display_connect_to_fd:
invlpg [rax]
db "wl_display_connect_to_fd", 0
ret

global wl_display_get_fd:function
align 16
wl_display_get_fd:
invlpg [rax]
db "wl_display_get_fd", 0
ret

global wl_event_queue_destroy:function
align 16
wl_event_queue_destroy:
invlpg [rax]
db "wl_event_queue_destroy", 0
ret

global wl_proxy_add_listener:function
align 16
wl_proxy_add_listener:
invlpg [rax]
db "wl_proxy_add_listener", 0
ret

global wl_proxy_create:function
align 16
wl_proxy_create:
invlpg [rax]
db "wl_proxy_create", 0
ret

global wl_proxy_destroy:function
align 16
wl_proxy_destroy:
invlpg [rax]
db "wl_proxy_destroy", 0
ret

global wl_proxy_create_wrapper:function
align 16
wl_proxy_create_wrapper:
invlpg [rax]
db "wl_proxy_create_wrapper", 0
ret

global wl_proxy_get_class:function
align 16
wl_proxy_get_class:
invlpg [rax]
db "wl_proxy_get_class", 0
ret

global wl_proxy_get_id:function
align 16
wl_proxy_get_id:
invlpg [rax]
db "wl_proxy_get_id", 0
ret

global wl_proxy_get_listener:function
align 16
wl_proxy_get_listener:
invlpg [rax]
db "wl_proxy_get_listener", 0
ret

global wl_proxy_get_tag:function
align 16
wl_proxy_get_tag:
invlpg [rax]
db "wl_proxy_get_tag", 0
ret

global wl_proxy_get_user_data:function
align 16
wl_proxy_get_user_data:
invlpg [rax]
db "wl_proxy_get_user_data", 0
ret

global wl_proxy_get_version:function
align 16
wl_proxy_get_version:
invlpg [rax]
db "wl_proxy_get_version", 0
ret

global wl_proxy_set_queue:function
align 16
wl_proxy_set_queue:
invlpg [rax]
db "wl_proxy_set_queue", 0
ret

global wl_proxy_set_tag:function
align 16
wl_proxy_set_tag:
invlpg [rax]
db "wl_proxy_set_tag", 0
ret

global wl_proxy_set_user_data:function
align 16
wl_proxy_set_user_data:
invlpg [rax]
db "wl_proxy_set_user_data", 0
ret

global wl_proxy_wrapper_destroy:function
align 16
wl_proxy_wrapper_destroy:
invlpg [rax]
db "wl_proxy_wrapper_destroy", 0
ret

global wl_proxy_marshal_array:function
align 16
wl_proxy_marshal_array:
invlpg [rax]
db "wl_proxy_marshal_array", 0
ret

global wl_proxy_marshal_array_constructor:function
align 16
wl_proxy_marshal_array_constructor:
invlpg [rax]
db "wl_proxy_marshal_array_constructor", 0
ret

global wl_proxy_marshal_array_constructor_versioned:function
align 16
wl_proxy_marshal_array_constructor_versioned:
invlpg [rax]
db "wl_proxy_marshal_array_constructor_versioned", 0
ret

global wl_proxy_marshal_array_flags:function
align 16
wl_proxy_marshal_array_flags:
invlpg [rax]
db "wl_proxy_marshal_array_flags", 0
ret

global wl_list_init:function
align 16
wl_list_init:
invlpg [rax]
db "wl_list_init", 0
ret

global wl_list_insert:function
align 16
wl_list_insert:
invlpg [rax]
db "wl_list_insert", 0
ret

global wl_list_remove:function
align 16
wl_list_remove:
invlpg [rax]
db "wl_list_remove", 0
ret

global wl_list_length:function
align 16
wl_list_length:
invlpg [rax]
db "wl_list_length", 0
ret

global wl_list_empty:function
align 16
wl_list_empty:
invlpg [rax]
db "wl_list_empty", 0
ret

global wl_list_insert_list:function
align 16
wl_list_insert_list:
invlpg [rax]
db "wl_list_insert_list", 0
ret


global wl_log_set_handler_client:function
align 16
wl_log_set_handler_client:
; TODO: callback stuff...
ret

section .init_array
    dq __felix86_constructor