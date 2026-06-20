# DebugFS

## Commands

```sh
mount -t debugfs none /sys/kernel/debug/    # Mount the debug fs
cat /sys/kernel/debug/dynamic_debug/control # Shows list of available messages
echo 'module serial +p' > /sys/kernel/debug/dynamic_debug/control
grep "serial" /sys/kernel/debug/dynamic_debug/control
echo 'module serial_module -p' > /sys/kernel/debug/dynamic_debug/control
echo 'file serial_core.c line 42 +p' > /sys/kernel/debug/dynamic_debug/control
echo 'func serial_rx_chars +p' > /sys/kernel/debug/dynamic_debug/control
```

```sh
# insmod drvbroken.ko
[ 3313.830109] 8<--- cut here ---
[ 3313.833427] Unable to handle kernel NULL pointer dereference at virtual address 00000000 when write
[ 3313.842582] [00000000] *pgd=81f85831, *pte=00000000, *ppte=00000000
[ 3313.848918] Internal error: Oops: 817 [#1] SMP ARM
[ 3313.853739] Modules linked in: drvbroken(O+) serial(O) [last unloaded: serial(O)]
[ 3313.861283] CPU: 0 PID: 147 Comm: insmod Tainted: G           O       6.7.12-00002-g8559a93f69d3 #14
[ 3313.870463] Hardware name: Generic AM33XX (Flattened Device Tree)
[ 3313.876585] PC is at mmioset+0x50/0xac
[ 3313.880386] LR is at 0x0
[ 3313.882933] pc : [<c0b7c490>]    lr : [<00000000>]    psr: 00000013
[ 3313.889228] sp : e00fde30  ip : 00000000  fp : c113d020
[ 3313.894476] r10: c1f98900  r9 : 000c7008  r8 : 00000000
[ 3313.899724] r7 : 00000000  r6 : c1110220  r5 : bf00b17c  r4 : 00000000
[ 3313.906281] r3 : 00000000  r2 : fffffffc  r1 : 00000000  r0 : 00000000
[ 3313.912839] Flags: nzcv  IRQs on  FIQs on  Mode SVC_32  ISA ARM  Segment none
[ 3313.920010] Control: 10c5387d  Table: 82cd0019  DAC: 00000051
[ 3313.925780] Register r0 information: NULL pointer
[ 3313.930515] Register r1 information: NULL pointer
[ 3313.935245] Register r2 information: non-paged memory
[ 3313.940323] Register r3 information: NULL pointer
[ 3313.945052] Register r4 information: NULL pointer
[ 3313.949781] Register r5 information: 1-page vmalloc region starting at 0xbf00b000 allocated at load_module+0x6e4/0x1dd0
[ 3313.960640] Register r6 information: non-slab/vmalloc memory
[ 3313.966331] Register r7 information: NULL pointer
[ 3313.971060] Register r8 information: NULL pointer
[ 3313.975790] Register r9 information: non-paged memory
[ 3313.980869] Register r10 information: slab filp start c1f98900 pointer offset 0 size 160
[ 3313.989026] Register r11 information: non-slab/vmalloc memory
[ 3313.994804] Register r12 information: NULL pointer
[ 3313.999621] Process insmod (pid: 147, stack limit = 0x3b2db29c)
[ 3314.005575] Stack: (0xe00fde30 to 0xe00fe000)
[ 3314.009956] de20:                                     c113d030 c032e648 bf009340 c2c0d200
[ 3314.018175] de40: c1110220 bf00d03c bf00d000 c0102350 c1f05340 6824696b 00000000 00000000
[ 3314.026392] de60: 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
[ 3314.034611] de80: 00000000 00000000 00000000 00000000 00000000 00000000 00000000 6824696b
[ 3314.042829] dea0: c1f98900 bf009040 c1f05340 000c7008 c1f98900 c01d7644 c1f98900 c113d030
[ 3314.051048] dec0: 00000000 c1f98900 000c7008 c01d9920 e00fdee4 7fffffff 00000000 00000002
[ 3314.059265] dee0: 00000000 e00ff000 e00ff3cf e00ff640 e00ff000 00007744 e0105e34 e0105be4
[ 3314.067483] df00: e01055cc 00000344 000003f4 000007bc 0000043b 00000000 000007ac 00000037
[ 3314.075700] df20: 00000038 0000001e 00000018 00000017 00000000 6824696b 00000001 c113d430
[ 3314.083918] df40: 00000000 c01d9d08 00000001 6824696b c113d150 c253b518 000c7008 c253b518
[ 3314.092136] df60: 00000000 c113d150 00000000 00000000 e00fdf70 e00fdf70 00000000 6824696b
[ 3314.100354] df80: 000a0000 000c7008 ffffffff bea2df3e 0000017b c01002ec c2c0d200 0000017b
[ 3314.108571] dfa0: 00000000 c0100080 000c7008 ffffffff 00000003 000c7008 00000000 bea2df3e
[ 3314.116790] dfc0: 000c7008 ffffffff bea2df3e 0000017b 00000000 00000000 b6fbb000 00000000
[ 3314.125008] dfe0: bea2dc88 bea2dc78 000298d4 b6f03020 60000010 00000003 00000000 00000000
[ 3314.133237]  mmioset from cdev_init+0x18/0x3c
[ 3314.137636]  cdev_init from init_module+0x3c/0x1000 [drvbroken]
[ 3314.143615]  init_module [drvbroken] from do_one_initcall+0x58/0x2e8
[ 3314.150026]  do_one_initcall from do_init_module+0x50/0x1e4
[ 3314.155642]  do_init_module from init_module_from_file+0x94/0xd4
[ 3314.161690]  init_module_from_file from sys_finit_module+0x184/0x304
[ 3314.168088]  sys_finit_module from ret_fast_syscall+0x0/0x58
[ 3314.173784] Exception stack(0xe00fdfa8 to 0xe00fdff0)
[ 3314.178861] dfa0:                   000c7008 ffffffff 00000003 000c7008 00000000 bea2df3e
[ 3314.187080] dfc0: 000c7008 ffffffff bea2df3e 0000017b 00000000 00000000 b6fbb000 00000000
[ 3314.195296] dfe0: bea2dc88 bea2dc78 000298d4 b6f03020
[ 3314.200376] Code: a8ac410a cafffff9 08bd8100 e3120020 (18ac410a) 
[ 3314.206636] ---[ end trace 0000000000000000 ]---
Segmentation fault
```

```sh
cd fs
arm-linux-gnueabi-objdump -DS char_dev.o > char_dev.S
```

```nasm
00000258 <cdev_init>:
{
 258:   e92d4070        push    {r4, r5, r6, lr}
        memset(cdev, 0, sizeof *cdev);
 25c:   e3a0203c        mov     r2, #60 @ 0x3c
{
 260:   e1a04000        mov     r4, r0
 264:   e1a05001        mov     r5, r1
        memset(cdev, 0, sizeof *cdev);
 268:   e3a01000        mov     r1, #0
 26c:   ebfffffe        bl      0 <memset>
        INIT_LIST_HEAD(&cdev->list);
 270:   e284302c        add     r3, r4, #44     @ 0x2c
 * Initializes the list_head to point to itself.  If it is a list header,
 * the result is an empty list.
 */
 ```


Map hex offset **`+0x18`** to this exact assembly output to see exactly where it triggered the crash.

---

### Step 1: Find the crashing instruction via Hex Math

The function starts at address `0x258`.
Add offset (`0x18`) to that base address:


$$\text{0x258} + \text{0x18} = \text{0x270}$$

Looking right at the disassembly output, address **`270`** is this exact line:

```nasm
 270:   e284302c        add     r3, r4, #44     @ 0x2c

```

---

### Step 2: Correlate it back to the Source Code

```C
        INIT_LIST_HEAD(&cdev->list);

```

### Step 3: Why did it crash here?

Let’s trace the registers up to that point:

1. At `260:`, the code copies `r0` (the first argument passed to `cdev_init`, which is your `struct cdev *` pointer) into register `r4`:
`mov r4, r0`
2. At `270:`, it tries to calculate the memory address of the list head (`&cdev->list`) by adding an offset of 44 bytes (`#44`) to whatever is in `r4`:
`add r3, r4, #44`

However, as we saw in your initial crash report, **`r0` (and consequently `r4`) was completely `NULL` (`0x00000000`)**.

Instead of pointing to a valid structure, the code tried to reference `0x00000000 + 44`, and the very next operations inside `INIT_LIST_HEAD` tried to write memory to that invalid destination.
