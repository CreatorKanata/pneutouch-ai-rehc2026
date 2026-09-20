"""Phase 0 defaults; prepare_vendor.py generates the matching firmware header."""

PROJECT_NAME = "PneuTouchSolistAI"
CPU_HZ = 48_000_000
UART_BAUD = 115_200
# Vendor Uart1.c's 48 MHz / 115200 configuration (not a generic UART divisor).
UART_DIVISOR = 0x0019
UART_ADJUST = 0x0019
SAMPLE_RATE_HZ = 40  # HX710B differential input supports 10 or 40 SPS.
SENSOR_TIMEOUT_MS = 1000
SETTLE_MS = 500
CLOCK_HALF_US = 2
STACK_BYTES = 2048
SERIAL_TIMEOUT_S = 0.1
NO_DATA_WARNING_S = 3.0
MAX_LINE_BYTES = 160
CONSOLE_PORT = 8000
READ_CHUNK_BYTES = 4096
DISPLAY_SAMPLES = 400
RECORDING_SAMPLES = 24_000  # Ten minutes at nominal 40 SPS; then stop recording.
# Pin the DT board package rather than choosing from unrelated RB sample projects.
VENDOR_PROJECT = (
    "REHC向けソフトウェア-2026-03-16/ソフトウェア/AIVibrationInferenceLexide/"
    "AIVibrationInferenceLexide/Workspace/AIVibrationInference"
)
CMSIS_PACK = "ARM.CMSIS.5.9.0.pack"
