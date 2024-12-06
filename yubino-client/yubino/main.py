import argparse
import logging
from .shell import YubinoShell

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--device", type=str, default="/dev/ttyACM0",
                        help="Connect to the given device, defaults to '/dev/ttyACM0'")
    parser.add_argument("-b", "--baud", help="Baud rate, defaults to 115200",
                        type=int, default=115200)
    parser.add_argument("-r", "--relying-party", default="http://localhost:8000", type=str,
                        help="Relying party to connect to, defaults to 'http://localhost:8000'")
    parser.add_argument("--list-devices", help="List available serial devices", action="store_true")
    parser.add_argument("-v", "--verbose", help="Verbose mode", action="store_true")
    args = parser.parse_args()

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

    if args.list_devices:
        for port in serial.tools.list_ports.comports():
            print(port.device)
        return 0

    return YubinoShell(args).cmdloop()

if __name__ == "__main__":
    main()
