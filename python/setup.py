#######################################################################
# Author       : Hans Erik Fjeld
#######################################################################
# All Rights Reserved


# import os
from setuptools import setup, find_packages

# dir_path = os.path.dirname(os.path.realpath(__file__))
# os.chdir(dir_path)

setup(
    name='ble_link',
    version='1.0.0',
    description='BLE Spp',
    long_description="Serves a null modem and virtual com port to ble spp",
    author='Hans Erik Fjeld',
    author_email='a@a.no',
    url='www.github.com/hanserf',
    packages=find_packages(),
    setup_requires=['wheel'],
    install_requires=['ble-serial'],#,'memory_profiler'],
    include_package_data=True,
    package_data={
        'rsep_tcp.static': ['*.json'],
    },
    entry_points={
        'console_scripts': [
        ],
    }
)


'''
    


'''
