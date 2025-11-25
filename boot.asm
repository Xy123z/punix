[BITS 16]
[ORG 0x7C00]

start:
    ; Setup segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Print loading message
    mov si, loading_msg
    call print_string

    ; Load kernel from disk into memory at 0x1000:0x0000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02        ; Read sectors
    mov al, 60         ; Number of sectors
    mov ch, 0           ; Cylinder 0
    mov cl, 2           ; Start from sector 2
    mov dh, 0           ; Head 0
    mov dl, 0           ; Drive 0
    int 0x13

    jc error            ; If error, jump to error handler

    ; Print success message
    mov si, success_msg
    call print_string

    ; Print starting message
    mov si, starting_msg
    call print_string

    ; Countdown: 3... 2... 1...
    mov cx, 3               ; Start countdown from 3
countdown_loop:
    ; Print the number
    mov al, cl
    add al, '0'             ; Convert to ASCII
    mov ah, 0x0E
    mov bh, 0
    int 0x10

    ; Print "... "
    mov si, dots_msg
    call print_string

    ; Delay approximately 1 second
    call delay_one_second

    dec cx
    jnz countdown_loop

    ; Print "GO!" message
    mov si, go_msg
    call print_string

    ; Small delay before jumping
    call delay_half_second

    ; Enable A20 line (allows access to memory above 1MB)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Switch to protected mode
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to protected mode
    jmp 0x08:protected_mode_start

error:
    mov si, error_msg
    call print_string
    hlt
    jmp $

print_string:
    push ax
    push bx
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; Delay approximately 1 second using BIOS timer
delay_one_second:
    push ax
    push cx
    push dx

    ; Get current tick count
    xor ah, ah
    int 0x1A                ; Returns tick count in CX:DX
    mov bx, dx              ; Save initial tick count
    add bx, 18              ; Add ~18 ticks (about 1 second, 18.2 Hz timer)

.wait_loop:
    xor ah, ah
    int 0x1A
    cmp dx, bx
    jl .wait_loop

    pop dx
    pop cx
    pop ax
    ret

; Delay approximately 0.5 seconds
delay_half_second:
    push ax
    push cx
    push dx

    xor ah, ah
    int 0x1A
    mov bx, dx
    add bx, 9               ; Add ~9 ticks (about 0.5 seconds)

.wait_loop:
    xor ah, ah
    int 0x1A
    cmp dx, bx
    jl .wait_loop

    pop dx
    pop cx
    pop ax
    ret

loading_msg db 'Loading kernel...', 13, 10, 0
success_msg db 'Kernel loaded!', 13, 10, 0
starting_msg db 13, 10, 'Starting kernel in: ', 0
dots_msg db '... ', 0
go_msg db 'GO!', 13, 10, 13, 10, 0
error_msg db 'ERROR: Disk read failed!', 13, 10, 0

; GDT - Global Descriptor Table
gdt_start:
    ; Null descriptor
    dq 0

    ; Code segment descriptor
    dw 0xFFFF       ; Limit 0-15
    dw 0x0000       ; Base 0-15
    db 0x00         ; Base 16-23
    db 10011010b    ; Access byte: present, ring 0, code, executable, readable
    db 11001111b    ; Flags + Limit 16-19: 4KB blocks, 32-bit
    db 0x00         ; Base 24-31

    ; Data segment descriptor
    dw 0xFFFF       ; Limit 0-15
    dw 0x0000       ; Base 0-15
    db 0x00         ; Base 16-23
    db 10010010b    ; Access byte: present, ring 0, data, writable
    db 11001111b    ; Flags + Limit 16-19: 4KB blocks, 32-bit
    db 0x00         ; Base 24-31

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size
    dd gdt_start                 ; Offset

[BITS 32]
protected_mode_start:
    ; Setup segment registers
    mov ax, 0x10        ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000    ; Setup stack

    ; Jump to kernel at 0x10000
    jmp 0x10000

; Boot signature
times 510-($-$$) db 0
dw 0xAA55
