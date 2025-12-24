[global enter_user_mode]

; void enter_user_mode(uint32_t target_eip, uint32_t target_esp)
enter_user_mode:
    ; Parameters: [esp+4] = EIP, [esp+8] = ESP
    
    cli
    mov ebx, [esp + 4]    ; target_eip
    mov eax, [esp + 8]    ; target_esp

    ; Setup segment registers for user mode
    ; Selector 0x20 is User Data (base 0, limit 4GB, RPL 3)
    ; 0x20 | 3 = 0x23
    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    ; Prepare the stack for IRET
    ; IRET expects: [SS, ESP, EFLAGS, CS, EIP]
    
    push 0x23             ; SS (User Data)
    push eax              ; ESP (User Stack)
    pushf                 ; EFLAGS
    
    ; Ensure IF (interrupt flag) is set in the pushed EFLAGS so interrupts 
    ; are enabled once we reach user mode.
    pop eax
    or eax, 0x200
    push eax

    push 0x1B             ; CS (User Code: 0x18 | 3)
    push ebx              ; EIP (Target function address)

    iret
