; This guarantees that the four param chunks of the key and IV are contiguous
; in memory.

    .module aes_params
    .globl _encryption_key
	.globl _encryption_IV
    .area CONST   (CODE)
_encryption_key:
G$param_encryption_key_0$0$0 == .
    .byte 0,0,0,0
G$param_encryption_key_1$0$0 == .
    .byte 0,0,0,0
G$param_encryption_key_2$0$0 == .
    .byte 0,0,0,0
G$param_encryption_key_3$0$0 == .
    .byte 0,0,0,0
_encryption_IV:
G$param_encryption_IV_0$0$0 == .
    .byte 0,0,0,0
G$param_encryption_IV_1$0$0 == .
    .byte 0,0,0,0
G$param_encryption_IV_2$0$0 == .
    .byte 0,0,0,0
G$param_encryption_IV_3$0$0 == .
    .byte 0,0,0,0
