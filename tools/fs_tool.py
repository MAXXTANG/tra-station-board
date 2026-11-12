"""Optional native mklittlefs override for Apple Silicon hosts."""
import os
from pathlib import Path

Import("env")
native = os.environ.get('TRA_MKLITTLEFS')
if native:
    tool = Path(native).expanduser().resolve()
    if not tool.is_file() or not os.access(tool, os.X_OK):
        raise RuntimeError('TRA_MKLITTLEFS must name an executable mklittlefs')
    env.Replace(MKFSTOOL=str(tool))
