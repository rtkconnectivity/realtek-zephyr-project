import abc
import argparse
import json
import os
import platform
import subprocess
import sys
from typing import Any, Dict, List, Type
from pathlib import Path

THIS_ZEPHYR = Path(__file__).parents[2] / 'zephyr'
ZEPHYR_BASE = Path(os.environ.get('ZEPHYR_BASE', THIS_ZEPHYR))

sys.path.insert(0, os.path.join(ZEPHYR_BASE, "scripts", "west_commands"))
from textwrap import dedent           
from west.commands import WestCommand 
from west.configuration import config
from west import log

from build_helpers import find_build_dir, is_zephyr_build, \
    FIND_BUILD_DIR_DESCRIPTION
from runners.core import BuildConfiguration

log.set_verbosity(log.VERBOSE_NONE)

def cmd_exec(cmd, cwd=None, shell=False):
    """Execute a command and return its output"""
    log.dbg(str(cmd))
    return subprocess.check_call(cmd, cwd=cwd, shell=shell)

class RealtekBee(WestCommand):

    def __init__(self):
        super().__init__(
            'realtek-bee',
            'Realtek tools for west framework',
            dedent('''
            Realtek tools for west framework'''))
        self.tool_classes: List[Type[Tool]] = [PrependHeader, MD5, MPCLI, PACKCLI]
 
    def check(self, cond, msg, exit) -> bool:
        if not cond:
            self.wrn(msg)
            if exit:
                sys.exit(1)
            return False
        else:
            return True

    def do_add_parser(self, parser_adder):
        """Add parser for Realtek Bee command"""
        parser = parser_adder.add_parser(self.name,
                                         help=self.help,
                                         description=self.description)
        parser.add_argument('-d', '--build-dir',
                            help=FIND_BUILD_DIR_DESCRIPTION)
        subparsers = parser.add_subparsers(dest="subcommand")
        for tool_class in self.tool_classes:
            tool_class.do_add_parser(subparsers)
        return parser

    def do_run(self, args, unknown_args):
        self.args = args

        # Find the build directory and parse .config and DT.
        build_dir = find_build_dir(args.build_dir)
        self.check(os.path.isdir(build_dir),
                    'no such build directory {}'.format(build_dir), True)
        if not self.check(is_zephyr_build(build_dir),
                "build directory {} doesn't look like a Zephyr build "
                'directory'.format(build_dir), False):
            build_dir = None

        if unknown_args:
            log.inf(f"unknown_args:{unknown_args}")
        # ToDo: get -b from IC type

        module_path = (
            Path(os.getenv("ZEPHYR_BASE")).absolute()
            / r".."
            / "realtek-zephyr-project"
        )
        ic_series = "Bee"
        tools_path = Path(module_path, f"tool/bin/{ic_series}")

        for tool_class in self.tool_classes:
            if args.subcommand == tool_class.name:
                print(f"Start {tool_class.name}")
                tool_instance = tool_class(tools_path, build_dir)
                tool_instance.do_run(args, unknown_args)

class Tool(abc.ABC):
    """Common abstract superclass for tools"""
    name = ""
    def __init__(self, tools_path, build_dir=None):
        self.tools_path = tools_path
        if build_dir:
            self.build_dir = Path(build_dir)
            self.build_conf = BuildConfiguration(build_dir)

            # load properties
            self.kernel = self.build_conf.get('CONFIG_KERNEL_BIN_NAME', 'zephyr')
            self.soc_series = self.build_conf.get('CONFIG_SOC_SERIES', 'none')
            self.in_bin = self.build_dir / 'zephyr' / f'{self.kernel}.bin'
        else:
            self.build_conf = None

    @classmethod
    @abc.abstractmethod
    def do_add_parser(self, parser):
        """Add parser for the tool"""

    @abc.abstractmethod
    def do_run(self, args, unknown_args):
        """Run the tool"""

    def forceable_build_conf(self, msg=""):
        """Check if build configuration is available"""
        if not self.build_conf:
            log.err("Zephyr build directory is required ", msg)
            sys.exit(1)

class PrependHeader(Tool):
    name = 'prepend'
             
    @classmethod
    def do_add_parser(cls, parser):
        prepend_parser = parser.add_parser(cls.name, help='prepend header')
        prepend_parser.add_argument('-b', '--ictype', default='16', help='ic type')
        return parser

    def do_run(self, args, unknown_args):
        self.forceable_build_conf()

        ini_path = Path(self.tools_path, "prepend_header/mp.ini")
        cmd_path = Path(os.getcwd())
        if platform.system() == 'Windows':
            prepend_header_path = Path(self.tools_path, "prepend_header/prepend_header.exe")
        elif platform.system() == 'Darwin':
            prepend_header_path = Path(self.tools_path, "prepend_header/prepend_header.mac")
        else:
            log.err('not supportted')

        cmd_args = (prepend_header_path, 
                    "-b", "16", 
                    "-t", "app_code",
                    "-m", "1", 
                    "-c", "sha256",
                    "-i", ini_path,
                    "-p", self.in_bin)
        cmd_exec(cmd_args, cwd=cmd_path)

class MD5(Tool):
    name = 'md5'

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_parser(cls.name, help='md5')
        return parser

    def do_run(self, args, unknown_args):
        # check params required
        self.forceable_build_conf()

        in_bin_mp_path = self.in_bin.with_name(self.in_bin.stem + "_MP" + self.in_bin.suffix)
        cmd_path = Path(os.getcwd())

        if platform.system() == 'Windows':
            md5_path = Path(self.tools_path, "md5/md5.exe")
        elif platform.system() == 'Darwin':
            md5_path = Path(self.tools_path, "md5/MD5.mac")
        else:
            log.err(f"{platform.system()} not supportted")

        cmd_exec((md5_path, in_bin_mp_path), cwd=cmd_path)

class MPCLI(Tool):
    name = 'mpcli'

    def __init__(self, tools_path, build_dir):
        super().__init__(tools_path, build_dir)
        self.port = ""
        self.baud = "1000000"
        self.relativepath = ""
        self.files: List[Dict[str, Any]] = []
    
    @classmethod
    def do_add_parser(cls, parser):
        mpcli_parser = parser.add_parser(cls.name, formatter_class=argparse.RawTextHelpFormatter,help='MPCLI tool for firmware downloading',)
        mpcli_parser.add_argument('-c', '--com-port', required=True, type=str, 
                                help='Serial communication port (e.g., COM3, /dev/ttyUSB0)')
        mpcli_parser.add_argument('-E', '--chip-erase', required=False, action='store_true', 
                                help='chip erase')

        group = mpcli_parser.add_argument_group(
            "target image",
            description=dedent('''
            Without any target image option, the default zephyr.bin from build directory will be used
            '''))

        group.add_argument('--bin-file', type=str, 
                        help='External firmware image in binary format. Must be used with --address option')

        group.add_argument('--address', type=str, 
                        help='Download address for external image (hex format, e.g., 0x8000000)')
        
        group.add_argument('--json', type=str, 
                        help=dedent('''
                        Configuration file (mptoolconfig.json) containing image path and download address.
                        Example format:
                        {
                            "mptoolconfig": {
                                "port": "",
                                "baud": "",
                                "appimage": {
                                    "relativepath": "",
                                    "file": [
                                        {
                                            "id": 0,
                                            "address": "0x00801000",
                                            "name": "fw1.bin",
                                            "enable": "1"
                                        }
                                    ]
                                }
                            }
                        }
                        '''))
        return parser
    
    def add_file(self, address: str, name: str,id: int = 0, enable: str = "1") -> None:
        file_item = {
            "id": id,
            "address": address,
            "name": name,
            "enable": enable
        }
        self.files.append(file_item)
    
    def export_to_file(self, filename: str) -> None:
        mptool_config = {
            "mptoolconfig": {
                "port": self.port,
                "baud": self.baud,
                "appimage": {
                    "relativepath": self.relativepath,
                    "file": self.files
                }
            }
        }
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(mptool_config, f, indent=4, ensure_ascii=False)

    @staticmethod
    def flash_address_from_build_conf(build_conf: BuildConfiguration):
        '''If CONFIG_HAS_FLASH_LOAD_OFFSET is n in build_conf,
        return the CONFIG_FLASH_BASE_ADDRESS value. Otherwise, return
        CONFIG_FLASH_BASE_ADDRESS + CONFIG_FLASH_LOAD_OFFSET.
        '''
        if build_conf.getboolean('CONFIG_HAS_FLASH_LOAD_OFFSET'):
            return (build_conf['CONFIG_FLASH_BASE_ADDRESS'] +
                    build_conf['CONFIG_FLASH_LOAD_OFFSET'])
        else:
            return build_conf['CONFIG_FLASH_BASE_ADDRESS']

    def do_run(self, args, unknown_args):
        self.port = args.com_port
        cmd_path = Path(os.getcwd())
        
        bin_file_path = ""
        download_address = ""
        mptoolconfig_path = ""
        if args.json: # specify images and address with json file
            mptoolconfig_path = Path(args.json)
            if not os.path.isfile(mptoolconfig_path):
                log.err('no such json file {}'.format(mptoolconfig_path))
        else:
            if args.bin_file: # download bin image
                if not args.address:
                    log.err("The download address needs to be specified with `--address` option")
                download_address = args.address
                bin_file_path = Path(args.bin_file)
                if not os.path.isfile(bin_file_path):
                    log.err('no such bin file {}'.format(bin_file_path))
            else:
                self.forceable_build_conf("for downloading zephyr bin")
                download_address = hex(self.flash_address_from_build_conf(self.build_conf))
                bin_file_path = self.build_dir / 'zephyr'/ f'{self.kernel}.bin'

            self.add_file(download_address, bin_file_path.name)
            mptoolconfig_path = bin_file_path.parent / "mptoolconfig.json"
            self.export_to_file(mptoolconfig_path)
        
        if platform.system() == 'Windows':
            mpcli_path = Path(self.tools_path, "mpcli/mpcli.exe")
        elif platform.system() == 'Darwin':
            mpcli_path = Path(self.tools_path, "mpcli/mpcli.mac")
        else:
            log.err('not supportted')
        cmd_args = (
                mpcli_path,
                "-c", self.port,
                "-f", mptoolconfig_path,
                "-a", "-r"
        )
        if args.chip_erase:
            cmd_args = cmd_args + ("-E",)
        try:
            cmd_exec(cmd_args, cwd=cmd_path)
        except Exception as e:
            print(e.args)

class PACKCLI(Tool):
    name = "packcli"

    def __init__(self, tools_path, build_dir, ):
        super().__init__(tools_path, build_dir)

    @classmethod
    def do_add_parser(cls, parser):
        '''
        :param parser
        '''
        packcli_parser = parser.add_parser(cls.name, formatter_class=argparse.RawTextHelpFormatter,help='packcli tool for firmware packing')
        packcli_parser.add_argument('-n', '--ic-type', required=True, type=str, 
                                help='IC Type (e.g. 8762C/8762D/8762E/8762G_VA/8762G_VB/8771HTV/8752H/8771GUV/8772GWP/8772G')
        packcli_parser.add_argument('-m', '--pack-mode', required=True, type=str,choices=['MP', 'OTA'],  
                                help='Select pack mode: MP or OTA')
        packcli_parser.add_argument('--raw', action='store_const', const="RAW",
                                help='Generate packed image in raw format')
        packcli_parser.add_argument('-s', '--src-folder', type=str, required=True,  
                                help='Source images folder, include: flash map.ini and images in bin format')
        packcli_parser.add_argument('-d', '--dst-folder', type=str, required=True,
                                help='Dest packed image folder')

    def do_run(self, args, unknown_args):
        cmd_path = Path(os.getcwd())

        src_dir_path = Path(args.src_folder)
        dst_dir_path = Path(args.dst_folder)
        if not os.path.isdir(src_dir_path):
            log.err('no such dir {}'.format(src_dir_path))
        if not os.path.isdir(dst_dir_path):
            log.err('no such dir {}'.format(dst_dir_path))
    
        if platform.system() == 'Windows':
            packcli_path = Path(self.tools_path, "packcli/PackCli.exe")
        elif platform.system() == 'Darwin':
            packcli_path = Path(self.tools_path, "packcli/PackCli.mac")
        else:
            log.err('not supportted')

        cmd_args = [
        packcli_path,
        "-n", args.ic_type,
        "-m", args.pack_mode,
        "-s", src_dir_path,
        "-d", dst_dir_path,
        ]
        if args.raw:
            cmd_args.append("--raw")
        try:
            cmd_exec(cmd_args, cwd=cmd_path)
        except Exception as e:
            print(e.args)
