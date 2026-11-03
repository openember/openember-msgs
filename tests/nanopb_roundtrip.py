"""Generate and test C bindings using an existing Nanopb checkout, without modifying it."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    nanopb, source = (Path(arg).resolve() for arg in sys.argv[1:3])
    compiler = sys.argv[3]
    with tempfile.TemporaryDirectory(prefix="openember-imu-nanopb-") as directory:
        output = Path(directory)
        protos = ["openember/msgs/common/v1/common", "openember/msgs/sensor/v1/sensor"]
        subprocess.run([sys.executable, str(nanopb / "generator/nanopb_generator.py"),
                        "-I", str(source / "proto"), "-f", str(source / "nanopb/openember_msgs.options"),
                        "-D", str(output), *[str(source / "proto" / (proto + ".proto")) for proto in protos]],
                       check=True, timeout=15)
        executable = output / "imu_nanopb_test"
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(nanopb), "-I", str(output),
                        str(source / "tests/imu_nanopb_test.c"),
                        *[str(output / (proto + ".pb.c")) for proto in protos],
                        *[str(nanopb / unit) for unit in ("pb_common.c", "pb_encode.c", "pb_decode.c")],
                        "-o", str(executable)], check=True, timeout=10)
        subprocess.run([str(executable)], check=True, timeout=2)


if __name__ == "__main__":
    main()
