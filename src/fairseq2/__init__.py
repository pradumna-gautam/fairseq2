# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

__version__ = "0.1.0.dev0"

__all__ = [
    "cuda_version",
    "get_cmake_prefix",
    "get_include",
    "get_lib",
    "supports_cuda",
]

# We import torch to ensure that libtorch.so is loaded into the process before
# our extension modules.
import torch  # noqa: F401

from fairseq2 import tbb

# We load TBB using our own lookup logic since it might be located in the
# Python virtual environment.
tbb._load()


from pathlib import Path
from typing import Optional, Tuple

from fairseq2 import _C


def supports_cuda() -> bool:
    """Indicates whether the library supports CUDA."""
    return _C._supports_cuda()


def cuda_version() -> Optional[Tuple[int, int]]:
    """Returns the version of CUDA with which the library was built.

    :returns:
        The major and minor version segments.
    """
    return _C._cuda_version()


def get_lib() -> Path:
    """Returns the directory that contains the fairseq2 shared library."""
    return Path(__file__).parent.joinpath("lib")


def get_include() -> Path:
    """Returns the directory that contains the fairseq2 header files."""
    return Path(__file__).parent.joinpath("include")


def get_cmake_prefix_path() -> Path:
    """Returns the directory that contains the fairseq2 CMake package."""
    return Path(__file__).parent.joinpath("lib", "cmake")
