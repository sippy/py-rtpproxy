from contextlib import suppress
from setuptools import setup, Extension, Command
from setuptools.command.build import build
import subprocess
import os

class CustomCommand(Command):
    def initialize_options(self) -> None:
        self.bdist_dir = None

    def finalize_options(self) -> None:
        with suppress(Exception):
            self.bdist_dir = self.get_finalized_command("bdist_wheel").bdist_dir

    def run(self):
        if self.bdist_dir:
            # Run the external build script
            subprocess.check_call(['sh', 'python/build_rtpproxy.sh'])

class CustomBuild(build):
    sub_commands = [('build_custom', None)] + build.sub_commands

def main():
    link_args = []
    compile_args = []
    compile_args.append('-Ibuild/include')
    for x in link_args, compile_args:
        x.append('-flto')
    smap_fname = 'python/symbols.map'
    link_args.append(f'-Wl,--version-script={smap_fname}')
    link_args.extend([
        '-Wl,--whole-archive',
        'rtpproxy/src/.libs/librtpproxy.a',
        '-Wl,--no-whole-archive',
        '-Lbuild/lib',
        '-l:libsrtp2.a', '-l:libssl.a', '-l:libcrypto.a',
    ])
    ext_modules = [
        Extension(
            'rtpproxy',
            sources=['python/rtpproxy_mod.c'],
            include_dirs=['rtpproxy/src'],  # adjust if headers are elsewhere
            extra_link_args=link_args,
            extra_compile_args=['-std=c99'],
        )
    ]

    setup(
        name='rtpproxy',
        version='0.1',
        description='Module to run RTPProxy inside Python.',
        ext_modules=ext_modules,
        cmdclass={
            'build': CustomBuild,
            'build_custom': CustomCommand,
        },
        classifiers=[
            'Programming Language :: Python :: 3',
            'Programming Language :: C',
        ],
    )

if __name__ == '__main__':
    main()
