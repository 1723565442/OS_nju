org	0100h
	mov ax, cs
    mov ds, ax
    mov es, ax
    call DispStr
    jmp $

DispStr:
    mov ax, BootMessage
    mov bp, ax  ;ES:BP 地址
    mov cx, 14  ;CX = 长度
    mov ax, 01301h ;AH = 13, AL = 01 表示执行显示字符窜功能，且光标在末尾
    mov bx, 004fh   ;BH = 0 页号=0 ， BL = 4f 红底白字高亮
    mov dl, 16 ;16列
	mov dh, 8  ;8行
    int 10h
    ret

BootMessage: db "Hello, Loader!"
