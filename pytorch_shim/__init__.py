
from .tieralloc_shim import enable, disable, hello, set_default_hint

from contextlib import contextmanager

@contextmanager
def fast_tier():
    prev = "warm"
    try:
        set_default_hint("pin_fast")
        yield
    finally:
        set_default_hint(prev)  # fallback

@contextmanager
def hot():
    prev = "warm"
    try: 
        set_default_hint("hot")
        yield
    finally: 
        set_default_hint(prev)

@contextmanager
def cold():
    prev = "warm"
    try: 
        set_default_hint("cold")
        yield
    finally:
        set_default_hint(prev)

