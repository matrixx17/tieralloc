from setuptools import setup
from torch.utils.cpp_extension import CppExtension, BuildExtension

setup(
    name='tieralloc_shim',
    ext_modules=[
        CppExtension(
            name='tieralloc_shim',
            sources=['tieralloc_shim.cpp'],
            include_dirs=['../include'],
            libraries=['tieralloc'],
            library_dirs=['../build'],
            extra_compile_args=['-std=gnu++20'],
        )
    ],
    cmdclass={'build_ext': BuildExtension.with_options(no_python_abi_suffix=True)},
    version='0.1.0',
    description='PyTorch CPU allocator shim for tieralloc library',
)
