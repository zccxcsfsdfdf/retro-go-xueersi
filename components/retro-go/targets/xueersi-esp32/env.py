# This file is injected late into rg_tool.py, you can run arbitrary python code here
# For example override python variables or set environment variables with os.putenv

# Espressif chip in the device
IDF_TARGET = "esp32"
# Raw ESP32 image; 4MB flash board
FW_FORMAT = "none"
# Fit 4MB flash by only building launcher + FC/GB/GBC core
DEFAULT_APPS = "launcher retro-core"
PROJECT_APPS = {
  'launcher':   [0, 16, 0x120000],
  'retro-core': [0, 17, 0x2C0000],
}
