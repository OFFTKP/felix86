bits 64

section .data

align 16
global wl_output_interface
wl_output_interface:
dq 0

global wl_shm_pool_interface
wl_shm_pool_interface:
dq 0

global wl_pointer_interface
wl_pointer_interface:
dq 0

global wl_compositor_interface
wl_compositor_interface:
dq 0

global wl_shm_interface
wl_shm_interface:
dq 0

global wl_registry_interface
wl_registry_interface:
dq 0

global wl_buffer_interface
wl_buffer_interface:
dq 0

global wl_seat_interface
wl_seat_interface:
dq 0

global wl_surface_interface
wl_surface_interface:
dq 0

global wl_keyboard_interface
wl_keyboard_interface:
dq 0

global wl_callback_interface
wl_callback_interface:
dq 0

wl_output_interface_name:
db "wl_output_interface", 0
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

libname:
db "libwayland-client.so", 0

section .text

global __felix86_constructor
align 16
__felix86_constructor:
invlpg [rbx]
ret
dd 0x12345678 ; invlpg + ret are 4 bytes, four more here to align to pointer
dq libname
; the constructor will set these to the host libwayland-client pointers
dq wl_output_interface_name
dq wl_output_interface
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
dq 0
dq 0

global wl_display_connect
align 16
wl_display_connect:
invlpg [rax]
db "wl_display_connect", 0
ret

global wl_display_flush
align 16
wl_display_flush:
invlpg [rax]
db "wl_display_flush", 0
ret

global wl_display_cancel_read
align 16
wl_display_cancel_read:
invlpg [rax]
db "wl_display_cancel_read", 0
ret

global wl_display_create_queue
align 16
wl_display_create_queue:
invlpg [rax]
db "wl_display_create_queue", 0
ret

global wl_display_disconnect
align 16
wl_display_disconnect:
invlpg [rax]
db "wl_display_disconnect", 0
ret

global wl_display_dispatch
align 16
wl_display_dispatch:
invlpg [rax]
db "wl_display_dispatch", 0
ret

global wl_display_dispatch_pending
align 16
wl_display_dispatch_pending:
invlpg [rax]
db "wl_display_dispatch_pending", 0
ret

global wl_display_dispatch_queue
align 16
wl_display_dispatch_queue:
invlpg [rax]
db "wl_display_dispatch_queue", 0
ret

global wl_display_dispatch_queue_pending
align 16
wl_display_dispatch_queue_pending:
invlpg [rax]
db "wl_display_dispatch_queue_pending", 0
ret

global wl_display_get_error
align 16
wl_display_get_error:
invlpg [rax]
db "wl_display_get_error", 0
ret

global wl_display_prepare_read
align 16
wl_display_prepare_read:
invlpg [rax]
db "wl_display_prepare_read", 0
ret

global wl_display_prepare_read_queue
align 16
wl_display_prepare_read_queue:
invlpg [rax]
db "wl_display_prepare_read_queue", 0
ret

global wl_display_read_events
align 16
wl_display_read_events:
invlpg [rax]
db "wl_display_read_events", 0
ret

global wl_display_roundtrip
align 16
wl_display_roundtrip:
invlpg [rax]
db "wl_display_roundtrip", 0
ret

global wl_display_roundtrip_queue
align 16
wl_display_roundtrip_queue:
invlpg [rax]
db "wl_display_roundtrip_queue", 0
ret

global wl_display_connect_to_fd
align 16
wl_display_connect_to_fd:
invlpg [rax]
db "wl_display_connect_to_fd", 0
ret

global wl_display_get_fd
align 16
wl_display_get_fd:
invlpg [rax]
db "wl_display_get_fd", 0
ret

global wl_event_queue_destroy
align 16
wl_event_queue_destroy:
invlpg [rax]
db "wl_event_queue_destroy", 0
ret

global wl_proxy_add_listener
align 16
wl_proxy_add_listener:
invlpg [rax]
db "wl_proxy_add_listener", 0
ret

global wl_proxy_create
align 16
wl_proxy_create:
invlpg [rax]
db "wl_proxy_create", 0
ret

global wl_proxy_destroy
align 16
wl_proxy_destroy:
invlpg [rax]
db "wl_proxy_destroy", 0
ret

global wl_proxy_create_wrapper
align 16
wl_proxy_create_wrapper:
invlpg [rax]
db "wl_proxy_create_wrapper", 0
ret

global wl_proxy_get_class
align 16
wl_proxy_get_class:
invlpg [rax]
db "wl_proxy_get_class", 0
ret

global wl_proxy_get_id
align 16
wl_proxy_get_id:
invlpg [rax]
db "wl_proxy_get_id", 0
ret

global wl_proxy_get_listener
align 16
wl_proxy_get_listener:
invlpg [rax]
db "wl_proxy_get_listener", 0
ret

global wl_proxy_get_tag
align 16
wl_proxy_get_tag:
invlpg [rax]
db "wl_proxy_get_tag", 0
ret

global wl_proxy_get_user_data
align 16
wl_proxy_get_user_data:
invlpg [rax]
db "wl_proxy_get_user_data", 0
ret

global wl_proxy_get_version
align 16
wl_proxy_get_version:
invlpg [rax]
db "wl_proxy_get_version", 0
ret

global wl_proxy_set_queue
align 16
wl_proxy_set_queue:
invlpg [rax]
db "wl_proxy_set_queue", 0
ret

global wl_proxy_set_tag
align 16
wl_proxy_set_tag:
invlpg [rax]
db "wl_proxy_set_tag", 0
ret

global wl_proxy_set_user_data
align 16
wl_proxy_set_user_data:
invlpg [rax]
db "wl_proxy_set_user_data", 0
ret

global wl_proxy_wrapper_destroy
align 16
wl_proxy_wrapper_destroy:
invlpg [rax]
db "wl_proxy_wrapper_destroy", 0
ret

global wl_proxy_marshal_array
align 16
wl_proxy_marshal_array:
invlpg [rax]
db "wl_proxy_marshal_array", 0
ret

global wl_proxy_marshal_array_constructor
align 16
wl_proxy_marshal_array_constructor:
invlpg [rax]
db "wl_proxy_marshal_array_constructor", 0
ret

global wl_proxy_marshal_array_constructor_versioned
align 16
wl_proxy_marshal_array_constructor_versioned:
invlpg [rax]
db "wl_proxy_marshal_array_constructor_versioned", 0
ret

global wl_proxy_marshal_array_flags
align 16
wl_proxy_marshal_array_flags:
invlpg [rax]
db "wl_proxy_marshal_array_flags", 0
ret

global wl_log_set_handler_client
align 16
wl_log_set_handler_client:
; TODO: callback stuff...
ret

section .init_array
    dq __felix86_constructor