from typing import Union, Sequence, TypeAlias
import numpy as np
import numpy.typing as npt

VectorInput: TypeAlias = Union[npt.NDArray[np.float32],
                               Sequence[float], Sequence[int]]
