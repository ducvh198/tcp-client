import os
import sys
import subprocess
import shutil

print("Python version:", sys.version)
for tool in ["gcc", "cl", "clang", "tcc", "make", "nmake", "mingw32-make"]:
    path = shutil.which(tool)
    print(f"Tool {tool}: {path}")

# Check if distutils/setuptools C compiler can be created
try:
    from setuptools import _distutils as distutils
    from distutils.ccompiler import new_compiler
    compiler = new_compiler()
    print("Default C compiler class:", compiler.__class__.__name__)
except Exception as e:
    print("C compiler error:", e)
