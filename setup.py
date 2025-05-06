from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import subprocess
import os

class CustomBuildExt(build_ext):
    def run(self):
        # Run the external build script
        subprocess.check_call(['sh', 'python/build_rtpproxy.sh'])
        # Proceed with normal extension building
        super().run()

def main():
    link_args = []
    compile_args = []
    compile_args.append('-flto')
    link_args.append('-flto')
    smap_fname = 'python/symbols.map'
    link_args.append(f'-Wl,--version-script={smap_fname}')

    ext_modules = [
        Extension(
            'rtpproxy',
            sources=['python/rtpproxy_mod.c'],
            include_dirs=['rtpproxy/src'],  # adjust if headers are elsewhere
            extra_objects=['rtpproxy/src/.libs/librtpproxy.a'],
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
            'build_ext': CustomBuildExt,
        },
        classifiers=[
            'Programming Language :: Python :: 3',
            'Programming Language :: C',
        ],
    )

if __name__ == '__main__':
    main()
