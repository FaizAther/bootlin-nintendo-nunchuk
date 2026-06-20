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