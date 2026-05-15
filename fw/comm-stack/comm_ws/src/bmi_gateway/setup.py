from setuptools import find_packages, setup

package_name = 'bmi_gateway'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=[
        'setuptools'],
    zip_safe=True,
    maintainer='rocknd79',
    maintainer_email='r.danylovych@gmail.com',
    description='TODO: Package description',
    license='TODO: License declaration',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'bmi_serial_bridge = bmi_gateway.bmi_serial_bridge:main'
        ],
    },
)
