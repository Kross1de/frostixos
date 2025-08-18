#!/bin/bash
# FrostixOS Build Status Script

echo "🎉 FrostixOS Build Status"
echo "================================"
echo

# Check if kernel exists
if [ -f "build/frostix.bin" ]; then
    echo "✅ Kernel: $(ls -lh build/frostix.bin | awk '{print $5}') (build/frostix.bin)"
else
    echo "❌ Kernel: Not built"
fi

# Check if ISO exists
if [ -f "frostix.iso" ]; then
    echo "✅ ISO: $(ls -lh frostix.iso | awk '{print $5}') (frostix.iso)"
else
    echo "❌ ISO: Not created"
fi

# Check multiboot compliance
if command -v grub-file >/dev/null 2>&1; then
    if grub-file --is-x86-multiboot build/frostix.bin >/dev/null 2>&1; then
        echo "✅ Multiboot: Compliant"
    else
        echo "❌ Multiboot: Not compliant"
    fi
else
    echo "⚠️  Multiboot: Cannot verify (grub-file not found)"
fi

# Count source files
c_files=$(find src/ -name "*.c" | wc -l)
s_files=$(find src/ -name "*.s" | wc -l)
h_files=$(find include/ -name "*.h" | wc -l)

echo "📁 Source Files: $c_files C files, $s_files assembly files, $h_files headers"

# Check if QEMU is available
if command -v qemu-system-i386 >/dev/null 2>&1; then
    echo "🖥️  QEMU: Ready to run (use 'make run')"
else
    echo "⚠️  QEMU: Not installed (ISO can still be used on real hardware)"
fi

echo
echo "🚀 Ready to boot! Try 'make run' or burn the ISO to test on real hardware."
echo "📖 See README.md for detailed usage instructions."
