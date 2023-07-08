    org 07c00h      ;告诉编译器加载到07c00h处
    mov ax, cs
    mov ds, ax
    mov es, ax
    call DispStr  ; 调用显示字符窜历程
    jmp $

DispStr:
    mov ax, BootMessage
    mov bp, ax      ;ES：BP = 串地址
    mov cx, 10      ;CX = 长度
    mov ax, 01301h  ;AH = 13， AL = 01h 
    mov bx, 000ch   ;BH = 0（页号为0）， BL = 0C（黑底红字高亮） 0-2：颜色 3：亮度 4-6：背景 7：闪烁
    mov dl, 0       ;第0列
    int 10h         ;
    ret

BootMessage: db "Hello, OS!"
times 510-($-$$) db 0  ;补齐为510B
dw 0xaa55       ;小端存储



