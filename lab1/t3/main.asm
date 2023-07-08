; nasm -f elf64 -o main.o main.asm
; ld -o main main.o
; ./main

section .data
    divident times 105 db 0
    divisor times 105 db 0
    quotient times 105 db 0 
    len_divident dd 0
    len_divisor dd 0
    len_quotient dd 0
    count_sub dd 10
    flag dd 0
    space db 32
    Quotient_0 : db "0 " 
    Error : db "Your divisor is 0, that's wrong !"
    Zero db 48

section .text
    global _start
_start:
    mov edi, divident ; 将 edi 指向字符串 1 的开头
.read_str1:
    mov eax, 3 ; 调用系统调用 3 以读取标准输入
    mov ebx, 0 ; 标准输入文件描述符为 0
    mov ecx, edi ; 将存储读取到的字符的缓存地址放入 ecx
    mov edx, 1 ; 一次只读取一个字符
    int 0x80 ; 调用系统调用
    cmp byte [edi], ' ' ; 字符是空格？
    je .read_str2 ; 跳转到读取字符串 2 的代码
    inc edi ; 向右移动字符指针
    add dword[len_divident], 1
    jmp .read_str1 ; 继续读取字符
.read_str2:
    mov byte [edi], 0 ; 在字符串 1 的结尾处添加 NULL
    mov edi, divisor ; 将 edi 指向字符串 2 的开头
.read_str2_loop:
    mov eax, 3 
    mov ebx, 0 
    mov ecx, edi 
    mov edx, 1 
    int 0x80 
    cmp byte [edi], 10 ; 换行符
    je .finish_read 
    inc edi 
    add dword[len_divisor], 1 
    jmp .read_str2_loop 
.finish_read:
    mov byte [edi], 0; 将字符串 2 的结尾处添加 NULL

.specialCase:
    mov ecx, [len_divident]
    mov edx, [len_divisor]
    mov eax, divisor
    cmp byte[eax], '0'
    je error 
    cmp ecx, edx
    jl quotient_zero
    
.start_division:
    mov eax, [len_divident]
    mov ebx, [len_divisor]
    sub eax, ebx
    inc eax
    mov [len_quotient], eax
    mov eax, divident
    xor ebx,ebx;ebx -- > i 最外层，作为商的位数的计数器
    loop1_start:
        cmp ebx, [len_quotient]
        jge outPut 
        xor ecx, ecx; ecx --> j 内层循环的计数器 是第i位商的具体数值
        loop2_start:
            cmp ecx, [count_sub]
            jge loop2_end
            call canSub
            cmp dword[flag], 1
            jne loop2_end
            call sub
            inc ecx
            jmp loop2_start
        loop2_end:
            add ecx, '0'
            mov edx, ecx
            mov byte[quotient + ebx], dl
            inc ebx
            inc eax
            jmp loop1_start
canSub:
    xor edx,edx 
    cmp ebx, 0
    je i_is_0
    cmp byte[eax -1], '0'
    jne return_1
i_is_0:
    xor esi, esi
loop_comp_start:
    cmp esi, [len_divisor]
    jge return_1
    mov dl, byte[eax + esi] ;取出divident[k]
    cmp dl, byte[divisor + esi]
    ja return_1
    cmp dl, byte[divisor + esi]
    jb return_0
    inc esi
    jmp loop_comp_start
return_1:
    mov dword[flag], 1
    jmp end_canSub
return_0:
    mov dword[flag], 0
end_canSub:
    ret 

sub:
    mov edi, [len_divisor]
    
    loop_sub_start:
        cmp edi, 0;
        je sub_end
        mov dl, byte[divisor + edi -1]
        sub byte[eax + edi - 1], dl
        add byte[eax + edi -1], '0'
        cmp byte[eax + edi - 1], '0'
        jb less0
        dec edi
        jmp loop_sub_start
    less0:
        add byte[eax + edi - 1], 10
        dec edi
        sub byte[eax + edi- 1], 1
        jmp loop_sub_start
    sub_end:
        ret 

outPut:
    ; 完成计算，输出商和余数,去除前导0
    mov dword[flag], 110
    mov ecx, quotient
    xor eax,eax 
loop3:   
    cmp eax, [len_quotient]
    jge end_quo_out
    cmp byte[ecx], '0'
    jne ne0
    inc ecx
    inc eax
    jmp loop3
ne0:
    mov dword[flag], eax

end_quo_out:
    cmp dword[flag], 110
    je out_quo_0
    mov eax,4
    mov edx, [len_quotient]
    sub edx, [flag]
    mov ebx, 1  
    int 0x80 
    jmp out_rem 
out_quo_0:
    mov eax ,4 
    mov edx, 1
    mov ecx, Zero 
    mov ebx , 1
    int 0x80
    
out_rem:
    mov eax ,4 
    mov edx, 1
    mov ecx, space  
    mov ebx , 1
    int 0x80

    mov dword[flag], 110
    mov ecx, divident
    xor eax,eax  
loop4:  
    cmp eax, [len_divident]
    jge end_rem_out
    cmp byte[ecx], '0'
    jne rem_ne0
    inc ecx
    inc eax
    jmp loop4
rem_ne0:
    mov dword[flag], eax
end_rem_out:
    cmp dword[flag], 110
    je out_rem_0
    mov eax,4
    mov edx, [len_divident]
    sub edx, [flag]
    mov ebx, 1  
    int 0x80 
    jmp exit
out_rem_0:
    mov eax ,4 
    mov edx, 1
    mov ecx, Zero  
    mov ebx , 1
    int 0x80
    jmp exit


error:
    mov eax ,4 
    mov edx, 33
    mov ecx, Error   
    mov ebx , 1
    int 0x80
    jmp exit

quotient_zero:
    mov eax ,4 
    mov edx, 2
    mov ecx, Quotient_0
    mov ebx , 1
    int 0x80
    mov eax ,4 
    mov edx, 105
    mov ecx, divident   
    mov ebx , 1
    int 0x80
    jmp exit

exit:
    mov     eax, 1           ; Linux 系统中的 exit 系统调用号
    xor     ebx, ebx         ; 返回值为 0，表示正常结束
    int     0x80            ; 调用 Linux 系统调用
