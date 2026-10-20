"""Immutable description of a chamber/channel/wall network."""
from dataclasses import dataclass
from typing import Optional, Tuple

from . import physics


@dataclass(frozen=True)
class Chamber:
    name: str
    volume_m3: float


@dataclass(frozen=True)
class Channel:
    """Hole or tube between chambers a and b (diameter d, length L in metres)."""
    a: int
    b: int
    d: float
    length: float

    @property
    def r(self):
        return physics.laminar_resistance(self.d, self.length)

    @property
    def k(self):
        return physics.orifice_coefficient(self.d)


@dataclass(frozen=True)
class Leak:
    """Path from a chamber to atmosphere with dp = r Q + k Q|Q|."""
    chamber: int
    r: float
    k: float


@dataclass(frozen=True)
class Wall:
    """Quasi-static wall piston: c x' = F - k x - area p.

    c_in applies while the wall moves into the chamber (x' > 0), c_out while
    it moves back out (snap-back after release).
    """
    chamber: int
    zone: int
    area: float
    k: float
    c_in: float
    c_out: float


@dataclass(frozen=True)
class Network:
    chambers: Tuple[Chamber, ...]
    channels: Tuple[Channel, ...]
    walls: Tuple[Wall, ...]
    leak: Optional[Leak]
    sensor: int

    def wall_index(self, zone):
        matches = [i for i, w in enumerate(self.walls) if w.zone == zone]
        if len(matches) != 1:
            raise ValueError(f'zone {zone} must map to exactly one wall, found {len(matches)}')
        return matches[0]

    def chamber_index(self, name):
        names = [c.name for c in self.chambers]
        if name not in names:
            raise ValueError(f'unknown chamber {name!r}; have {names}')
        return names.index(name)
