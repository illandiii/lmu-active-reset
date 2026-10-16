"""Owned constraint preparation ABI. Indices are local slots, never pointers."""

import ctypes as C


class BarGeometry(C.Structure):
    _fields_ = [("anchors_local", (C.c_double * 3) * 2), ("softness", C.c_double)]


class BallGeometry(C.Structure):
    _fields_ = [("anchors_local", (C.c_double * 3) * 2), ("softness", C.c_double * 3)]


class AngularGeometry(C.Structure):
    _fields_ = [("axes_local", (C.c_double * 3) * 3), ("softness", C.c_double * 3)]


class Geometry(C.Union):
    _fields_ = [("bar", BarGeometry), ("ball", BallGeometry), ("angular", AngularGeometry)]


class Recipe(C.Structure):
    _fields_ = [
        ("identity", C.c_size_t),
        ("body_indices", C.c_size_t * 2),
        ("kind", C.c_uint),
        ("geometry", Geometry),
    ]


class Prepared(C.Structure):
    _fields_ = [
        ("length", C.c_double),
        ("jacobian", (C.c_double * 3) * 4),
        ("weighted", (C.c_double * 3) * 4),
        ("inverse_effective_mass", C.c_double),
    ]


class Result(C.Structure):
    _fields_ = [
        ("identity", C.c_size_t),
        ("body_indices", C.c_size_t * 2),
        ("row_count", C.c_size_t),
        ("kind", C.c_uint),
        ("rows", Prepared * 3),
    ]


assert C.sizeof(Recipe) == 128 and C.sizeof(Result) == 664


class RowMemory(C.Structure):
    _fields_ = [(n, C.c_double) for n in ("impulse", "lower", "upper")]


class Memory(C.Structure):
    _fields_ = [
        ("identity", C.c_size_t),
        ("body_indices", C.c_size_t * 2),
        ("row_count", C.c_size_t),
        ("kind", C.c_uint),
        ("rows", RowMemory * 3),
    ]


assert C.sizeof(Memory) == 112
