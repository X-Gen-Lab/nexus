"""
Setup script for validation framework
"""

from setuptools import setup, find_packages

setup(
    name="nexus-validation",
    version="1.0.0",
    packages=find_packages(),
    install_requires=[
        "pytest>=7.4.0",
        "pytest-cov>=4.1.0",
        "hypothesis>=6.82.0",
        "coverage>=7.2.0",
        "jinja2>=3.1.2",
        "junit-xml>=1.9",
        "pyyaml>=6.0",
    ],
)
