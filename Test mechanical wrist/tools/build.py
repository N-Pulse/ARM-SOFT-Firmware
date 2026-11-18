"""Standalone build, including when the project path contains spaces. Never flashes."""
from pathlib import Path
import argparse
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = (ROOT / "build").resolve()
HAL = ROOT / "vendor/Drivers/STM32G4xx_HAL_Driver"

def clean():
    if OUT.parent != ROOT or OUT.name != "build":
        raise ValueError("Unsafe build directory")
    if OUT.exists():
        shutil.rmtree(OUT)

def main():
    parser = argparse.ArgumentParser(__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--rebuild", action="store_true")
    group.add_argument("--clean", action="store_true")
    args = parser.parse_args()
    if args.clean or args.rebuild:
        clean()
    if args.clean:
        return
    cc = shutil.which("arm-none-eabi-gcc")
    if not cc:
        raise SystemExit("Install ARM GNU toolchain and add its bin directory to PATH")
    cpu = ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb"]
    includes = [ROOT / p for p in ("Core/Inc", "firmware", "vendor/Drivers/CMSIS/Include",
                "vendor/Drivers/CMSIS/Device/ST/STM32G4xx/Include")]
    includes += [HAL / "Inc", HAL / "Inc/Legacy"]
    flags = [*cpu, "-DUSE_HAL_DRIVER", "-DSTM32G474xx", "-g3", "-Og", "-std=gnu11",
             "-Wall", "-Wextra", "-Werror", "-ffunction-sections", "-fdata-sections",
             "--specs=nano.specs", *["-I" + str(p) for p in includes]]
    modules = "hal hal_cortex hal_dma hal_dma_ex hal_exti hal_flash hal_flash_ex hal_flash_ramfunc hal_gpio hal_pwr hal_pwr_ex hal_rcc hal_rcc_ex hal_tim hal_tim_ex hal_uart hal_uart_ex".split()
    sources = sorted((ROOT / "Core/Src").glob("*.c")) + sorted((ROOT / "firmware").glob("*.c"))
    sources += [HAL / "Src" / f"stm32g4xx_{name}.c" for name in modules]
    sources += [ROOT / "Core/Startup/startup_stm32g474retx.s"]
    OUT.mkdir(exist_ok=True)
    objects = []
    # Recompile each source: simple and reliable after ANY header/config change.
    for source in sources:
        obj = OUT / "obj" / source.relative_to(ROOT).with_suffix(".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        command = [cc, *flags]
        if source.suffix == ".s":
            command += ["-x", "assembler-with-cpp"]
        subprocess.run([*command, "-c", str(source), "-o", str(obj)], cwd=ROOT, check=True)
        objects.append(obj)
    elf = OUT / "wrist-bench.elf"
    # Response-file quoting is required for the project's spaces on Windows.
    response = OUT / "objects.list"
    response.write_text("\n".join('"' + p.as_posix() + '"' for p in objects), encoding="utf-8")
    subprocess.run([cc, *cpu, "-o", str(elf), "@" + str(response), "-T" + str(ROOT / "STM32G474RETX_FLASH.ld"),
                    "--specs=nosys.specs", "--specs=nano.specs", "-Wl,--gc-sections",
                    "-Wl,-u,_printf_float", "-Wl,-Map=" + str(OUT / "wrist-bench.map"),
                    "-static", "-Wl,--start-group", "-lc", "-lm", "-Wl,--end-group"], cwd=ROOT, check=True)
    bin_dir = Path(cc).parent
    suffix = ".exe" if Path(cc).suffix == ".exe" else ""
    for extension, fmt in (("hex", "ihex"), ("bin", "binary")):
        subprocess.run([str(bin_dir / ("arm-none-eabi-objcopy" + suffix)), "-O", fmt,
                        str(elf), str(elf.with_suffix("." + extension))], check=True)
    subprocess.run([str(bin_dir / ("arm-none-eabi-size" + suffix)), str(elf)], check=True)
    print("Built build/wrist-bench.elf, .hex, .bin; no device programmed")

if __name__ == "__main__":
    main()
