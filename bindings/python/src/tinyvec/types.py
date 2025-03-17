from typing import Union, Sequence, Dict, Any, List
import numpy as np

VectorInput = Union[np.ndarray, Sequence[float], Sequence[int]]
JsonValue = Union[str, int, float, bool, None, Dict[str, Any], List[Any]]
